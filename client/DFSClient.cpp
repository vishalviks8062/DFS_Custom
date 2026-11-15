#include "DFSClient.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>

namespace fs = std::filesystem;

namespace {
constexpr std::size_t CHUNK_SIZE = 64 * 1024;
}

DFSClient::DFSClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(dfs::FileService::NewStub(channel)) {

    fs::create_directories(localDirectory_);
}


// 1. STORE FILE (Client Streaming)

grpc::Status DFSClient::Store(const std::string& filename) {

    fs::path filePath = fs::path(localDirectory_) / filename;

    std::ifstream input(filePath, std::ios::binary);

    if (!input.is_open()) {
        return grpc::Status(
            grpc::StatusCode::NOT_FOUND,
            "Local file not found"
        );
    }

    grpc::ClientContext context;
    dfs::StoreResponse response;

    std::unique_ptr<grpc::ClientWriter<dfs::FileChunk>> writer(
        stub_->Store(&context, &response)
    );

    std::array<char, CHUNK_SIZE> buffer;

    while (input) {

        input.read(buffer.data(), buffer.size());

        std::streamsize bytesRead = input.gcount();

        if (bytesRead > 0) {

            dfs::FileChunk chunk;

            chunk.set_filename(filename);
            chunk.set_data(buffer.data(), static_cast<std::size_t>(bytesRead));

            if (!writer->Write(chunk)) {
                break;
            }
        }
    }

    writer->WritesDone();

    grpc::Status status = writer->Finish();

    if (status.ok()) {
        std::cout << "Uploaded: "
                  << response.filename() << std::endl;
    }

    return status;
}


// 2. FETCH FILE (Server Streaming)

grpc::Status DFSClient::Fetch(const std::string& filename) {

    grpc::ClientContext context;

    dfs::FileRequest request;
    request.set_filename(filename);

    std::unique_ptr<grpc::ClientReader<dfs::FileChunk>> reader(
        stub_->Fetch(&context, request)
    );

    fs::path filePath = fs::path(localDirectory_) / filename;

    std::ofstream output(filePath, std::ios::binary);

    if (!output.is_open()) {
        return grpc::Status(
            grpc::StatusCode::INTERNAL,
            "Cannot create local file"
        );
    }

    dfs::FileChunk chunk;

    while (reader->Read(&chunk)) {
        output.write(chunk.data().data(), chunk.data().size());
    }

    output.close();

    grpc::Status status = reader->Finish();

    if (status.ok()) {
        std::cout << "Downloaded: " << filename << std::endl;
    }

    return status;
}


// 3. DELETE FILE

grpc::Status DFSClient::Delete(const std::string& filename) {

    grpc::ClientContext context;

    dfs::FileRequest request;
    request.set_filename(filename);

    dfs::DeleteResponse response;

    grpc::Status status = stub_->Delete(
        &context,
        request,
        &response
    );

    if (status.ok()) {
        std::cout << "Deleted: "
                  << response.filename() << std::endl;
    }

    return status;
}


// 4. LIST FILES

grpc::Status DFSClient::List() {

    grpc::ClientContext context;

    dfs::ListRequest request;
    dfs::ListResponse response;

    grpc::Status status = stub_->List(
        &context,
        request,
        &response
    );

    if (status.ok()) {

        std::cout << "\nFiles on server:\n";

        for (const auto& file : response.files()) {

            std::cout << "Name: " << file.filename()
                      << " | Size: " << file.size()
                      << " bytes"
                      << " | Modified: " << file.modified_time()
                      << std::endl;
        }
    }

    return status;
}


// 5. GET FILE METADATA

grpc::Status DFSClient::Stat(const std::string& filename) {

    grpc::ClientContext context;

    dfs::FileRequest request;
    request.set_filename(filename);

    dfs::FileInfo response;

    grpc::Status status = stub_->Stat(
        &context,
        request,
        &response
    );

    if (status.ok()) {

        std::cout << "\nFile metadata:\n";

        std::cout << "Name: " << response.filename() << '\n';
        std::cout << "Size: " << response.size() << " bytes\n";
        std::cout << "Modified: " << response.modified_time() << '\n';
        std::cout << "Created: " << response.created_time() << '\n';
    }

    return status;
}