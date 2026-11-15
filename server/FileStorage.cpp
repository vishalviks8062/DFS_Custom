
#include "FileStorage.h"

#include <stdexcept>
#include <sys/stat.h>

namespace fs = std::filesystem;

// Constructor
FileStorage::FileStorage(const std::string& directory)
    : root_(fs::absolute(directory)) {
    fs::create_directories(root_);
}

// Validate filename
bool FileStorage::validFilename(const std::string& filename) const {
    if (filename.empty() || filename == "." || filename == "..") {
        return false;
    }

    return filename.find('/') == std::string::npos &&
           filename.find('\\') == std::string::npos &&
           filename.find('\0') == std::string::npos;
}

// Construct complete file path
fs::path FileStorage::filePath(const std::string& filename) const {
    if (!validFilename(filename)) {
        throw std::invalid_argument("Invalid filename");
    }

    fs::path path = root_ / filename;

    if (fs::is_symlink(fs::symlink_status(path))) {
        throw std::invalid_argument("Symbolic links are not allowed");
    }

    return path;
}

// Delete file
bool FileStorage::remove(const std::string& filename) {
    fs::path path = filePath(filename);

    if (!fs::is_regular_file(path)) {
        return false;
    }

    return fs::remove(path);
}

// Get file metadata
bool FileStorage::metadata(
    const std::string& filename,
    FileMetadata& info
) const {
    fs::path path = filePath(filename);

    if (!fs::is_regular_file(path)) {
        return false;
    }

    struct stat st {};

    if (::stat(path.c_str(), &st) != 0) {
        return false;
    }

    info.name = filename;
    info.size = static_cast<std::uint64_t>(st.st_size);
    info.modifiedTime = static_cast<std::int64_t>(st.st_mtime);
    info.createdTime = 0;

    return true;
}

// List all files
std::vector<FileMetadata> FileStorage::list() const {
    std::vector<FileMetadata> files;

    for (const auto& entry : fs::directory_iterator(root_)) {
        if (!entry.is_regular_file() || entry.is_symlink()) {
            continue;
        }

        FileMetadata info;

        if (metadata(entry.path().filename().string(), info)) {
            files.push_back(info);
        }
    }

    return files;
}
