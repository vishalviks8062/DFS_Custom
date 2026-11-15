#include "FileServiceImpl.h"

#include <array>
#include <fstream>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {
constexpr std::size_t CHUNK_SIZE = 64 * 1024;

void FillFileInfo(const FileMetadata& metadata, dfs::FileInfo* response) {
    response->set_filename(metadata.name);
    response->set_size(metadata.size);
    response->set_modified_time(metadata.modifiedTime);
    response->set_created_time(metadata.createdTime);
}
}

FileServiceImpl::FileServiceImpl(const std::string& directory)
    : storage_(directory) {}


// 1. STORE FILE (Client Streaming)

grpc::Status FileServiceImpl::Store(
    grpc::ServerContext* context,
    grpc::ServerReader<dfs::FileChunk>* reader,
    dfs::StoreResponse* response) {

    dfs::FileChunk chunk;
    std::string filename;
    bool firstChunk = true;

    fs::path temporaryPath;
    std::ofstream output;

    while (reader->Read(&chunk)) {

        if (context->IsCancelled()) {
            if (output.is_open()) output.close();

            if (!temporaryPath.empty()) {
                std::error_code ec;
                fs::remove(temporaryPath, ec);
            }

            return grpc::Status(
                grpc::StatusCode::CANCELLED,
                "Upload cancelled"
            );
        }

        if (firstChunk) {

            filename = chunk.filename();

            if (!storage_.validFilename(filename)) {
                return grpc::Status(
                    grpc::StatusCode::INVALID_ARGUMENT,
                    "Invalid filename"
                );
            }

            try {
                fs::path destination = storage_.filePath(filename);

                temporaryPath = destination;
                temporaryPath += ".uploading";

                output.open(temporaryPath, std::ios::binary | std::ios::trunc);

                if (!output.is_open()) {
                    return grpc::Status(
                        grpc::StatusCode::INTERNAL,
                        "Cannot create temporary file"
                    );
                }

            } catch (const fs::filesystem_error& e) {
                return grpc::Status(
                    grpc::StatusCode::INTERNAL,
                    e.what()
                );
            } catch (const std::invalid_argument& e) {
                return grpc::Status(
                    grpc::StatusCode::INVALID_ARGUMENT,
                    e.what()
                );
            }

            firstChunk = false;
        }

        if (chunk.filename() != filename) {
            output.close();

            std::error_code ec;
            fs::remove(temporaryPath, ec);

            return grpc::Status(
                grpc::StatusCode::INVALID_ARGUMENT,
                "Filename changed during upload"
            );
        }

        output.write(chunk.data().data(), chunk.data().size());

        if (!output.good()) {
            output.close();

            std::error_code ec;
            fs::remove(temporaryPath, ec);

            return grpc::Status(
                grpc::StatusCode::INTERNAL,
                "Error writing file"
            );
        }
    }

    if (firstChunk) {
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            "No file chunks received"
        );
    }

    output.close();

    if (context->IsCancelled()) {
        std::error_code ec;
        fs::remove(temporaryPath, ec);

        return grpc::Status(
            grpc::StatusCode::CANCELLED,
            "Upload cancelled"
        );
    }

    try {
        fs::rename(temporaryPath, storage_.filePath(filename));

        FileMetadata metadata;

        if (!storage_.metadata(filename, metadata)) {
            return grpc::Status(
                grpc::StatusCode::INTERNAL,
                "Cannot retrieve uploaded file metadata"
            );
        }

        response->set_filename(filename);
        response->set_modified_time(metadata.modifiedTime);

    } catch (const fs::filesystem_error& e) {
        std::error_code ec;
        fs::remove(temporaryPath, ec);

        return grpc::Status(
            grpc::StatusCode::INTERNAL,
            e.what()
        );
    } catch (const std::invalid_argument& e) {
        std::error_code ec;
        fs::remove(temporaryPath, ec);

        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            e.what()
        );
    }

    return grpc::Status::OK;
}


// 2. FETCH FILE (Server Streaming)

grpc::Status FileServiceImpl::Fetch(
    grpc::ServerContext* context,
    const dfs::FileRequest* request,
    grpc::ServerWriter<dfs::FileChunk>* writer) {

    const std::string filename = request->filename();

    if (!storage_.validFilename(filename)) {
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            "Invalid filename"
        );
    }

    fs::path path;

    try {
        path = storage_.filePath(filename);
    } catch (const std::exception& e) {
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            e.what()
        );
    }

    std::ifstream input(path, std::ios::binary);

    if (!input.is_open()) {
        return grpc::Status(
            grpc::StatusCode::NOT_FOUND,
            "File not found or cannot be opened"
        );
    }

    std::array<char, CHUNK_SIZE> buffer;

    while (input) {

        if (context->IsCancelled()) {
            return grpc::Status(
                grpc::StatusCode::CANCELLED,
                "Download cancelled"
            );
        }

        input.read(buffer.data(), buffer.size());

        std::streamsize bytesRead = input.gcount();

        if (bytesRead > 0) {

            dfs::FileChunk chunk;

            chunk.set_filename(filename);
            chunk.set_data(buffer.data(), static_cast<std::size_t>(bytesRead));

            if (!writer->Write(chunk)) {
                return grpc::Status(
                    grpc::StatusCode::CANCELLED,
                    "Client stopped receiving"
                );
            }
        }
    }

    if (input.bad()) {
        return grpc::Status(
            grpc::StatusCode::INTERNAL,
            "Error reading file"
        );
    }

    return grpc::Status::OK;
}


// 3. DELETE FILE

grpc::Status FileServiceImpl::Delete(
    grpc::ServerContext* context,
    const dfs::FileRequest* request,
    dfs::DeleteResponse* response) {

    const std::string filename = request->filename();

    if (!storage_.validFilename(filename)) {
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            "Invalid filename"
        );
    }

    try {

        if (!storage_.remove(filename)) {
            return grpc::Status(
                grpc::StatusCode::NOT_FOUND,
                "File not found"
            );
        }

    } catch (const std::invalid_argument& e) {
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            e.what()
        );
    } catch (const fs::filesystem_error& e) {
        return grpc::Status(
            grpc::StatusCode::INTERNAL,
            e.what()
        );
    }

    response->set_filename(filename);

    return grpc::Status::OK;
}


// 4. LIST FILES

grpc::Status FileServiceImpl::List(
    grpc::ServerContext* context,
    const dfs::ListRequest* request,
    dfs::ListResponse* response) {

    try {

        std::vector<FileMetadata> files = storage_.list();

        for (const auto& metadata : files) {

            if (context->IsCancelled()) {
                return grpc::Status(
                    grpc::StatusCode::CANCELLED,
                    "Request cancelled"
                );
            }

            dfs::FileInfo* info = response->add_files();

            FillFileInfo(metadata, info);
        }

    } catch (const fs::filesystem_error& e) {
        return grpc::Status(
            grpc::StatusCode::INTERNAL,
            e.what()
        );
    }

    return grpc::Status::OK;
}


// 5. GET FILE METADATA

grpc::Status FileServiceImpl::Stat(
    grpc::ServerContext* context,
    const dfs::FileRequest* request,
    dfs::FileInfo* response) {

    const std::string filename = request->filename();

    if (!storage_.validFilename(filename)) {
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            "Invalid filename"
        );
    }

    FileMetadata metadata;

    try {

        if (!storage_.metadata(filename, metadata)) {
            return grpc::Status(
                grpc::StatusCode::NOT_FOUND,
                "File not found"
            );
        }

    } catch (const std::invalid_argument& e) {
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            e.what()
        );
    } catch (const fs::filesystem_error& e) {
        return grpc::Status(
            grpc::StatusCode::INTERNAL,
            e.what()
        );
    }

    FillFileInfo(metadata, response);

    return grpc::Status::OK;
}