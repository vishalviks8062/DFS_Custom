#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "FileServiceImpl.h"

void RunServer() {

    const std::string serverAddress = "0.0.0.0:50052";
    const std::string storageDirectory = "server_files";

    FileServiceImpl service(storageDirectory);

    grpc::ServerBuilder builder;

    builder.AddListeningPort(
        serverAddress,
        grpc::InsecureServerCredentials()
    );

    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();

    if (!server) {
        std::cerr << "Failed to start DFS server" << std::endl;
        return;
    }

    std::cout << "DFS Server running at "
              << serverAddress << std::endl;

    std::cout << "Storage directory: "
              << storageDirectory << std::endl;

    server->Wait();
}

int main() {
    RunServer();
    return 0;
}