#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

#include "dfs_service.grpc.pb.h"

class DFSClient {
public:
    explicit DFSClient(std::shared_ptr<grpc::Channel> channel);

    grpc::Status Store(const std::string& filename);
    grpc::Status Fetch(const std::string& filename);
    grpc::Status Delete(const std::string& filename);
    grpc::Status List();
    grpc::Status Stat(const std::string& filename);

private:
    std::unique_ptr<dfs::FileService::Stub> stub_;

    const std::string localDirectory_ = "client_files";
};