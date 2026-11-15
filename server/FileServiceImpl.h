#pragma once

#include <string>
#include <grpcpp/grpcpp.h>

#include "dfs_service.grpc.pb.h"
#include "FileStorage.h"

class FileServiceImpl final : public dfs::FileService::Service {
public:
    explicit FileServiceImpl(const std::string& directory);

    grpc::Status Store(
        grpc::ServerContext* context,
        grpc::ServerReader<dfs::FileChunk>* reader,
        dfs::StoreResponse* response
    ) override;

    grpc::Status Fetch(
        grpc::ServerContext* context,
        const dfs::FileRequest* request,
        grpc::ServerWriter<dfs::FileChunk>* writer
    ) override;

    grpc::Status Delete(
        grpc::ServerContext* context,
        const dfs::FileRequest* request,
        dfs::DeleteResponse* response
    ) override;

    grpc::Status List(
        grpc::ServerContext* context,
        const dfs::ListRequest* request,
        dfs::ListResponse* response
    ) override;

    grpc::Status Stat(
        grpc::ServerContext* context,
        const dfs::FileRequest* request,
        dfs::FileInfo* response
    ) override;

private:
    FileStorage storage_;
};
