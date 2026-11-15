#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct FileMetadata {
    std::string name;
    std::uint64_t size;
    std::int64_t modifiedTime;
    std::int64_t createdTime;
};

class FileStorage {
public:
    explicit FileStorage(const std::string& directory);

    bool validFilename(const std::string& filename) const;

    std::filesystem::path filePath(
        const std::string& filename
    ) const;

    bool remove(const std::string& filename);

    bool metadata(
        const std::string& filename,
        FileMetadata& info
    ) const;

    std::vector<FileMetadata> list() const;

private:
    std::filesystem::path root_;
};