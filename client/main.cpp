#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "DFSClient.h"

void PrintUsage() {
    std::cout << "Usage:\n"
              << "  dfs_client <command> [filename] [server_address]\n\n"
              << "Commands:\n"
              << "  store <filename>\n"
              << "  fetch <filename>\n"
              << "  delete <filename>\n"
              << "  stat <filename>\n"
              << "  list\n\n"
              << "Default server: localhost:50052\n";
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::string command = argv[1];

    bool needsFilename = (command == "store" ||
                          command == "fetch" ||
                          command == "delete" ||
                          command == "stat");

    if ((!needsFilename && command != "list") ||
        (needsFilename && argc < 3) ||
        (needsFilename && argc > 4) ||
        (!needsFilename && argc > 3)) {
        PrintUsage();
        return 1;
    }

    std::string filename;
    if (needsFilename) {
        filename = argv[2];
    }

    std::string serverAddress = "localhost:50052";

    if (needsFilename && argc == 4) {
        serverAddress = argv[3];
    } else if (!needsFilename && argc == 3) {
        serverAddress = argv[2];
    }

    auto channel = grpc::CreateChannel(
        serverAddress,
        grpc::InsecureChannelCredentials()
    );

    DFSClient client(channel);

    grpc::Status status;

    if (command == "store") {
        status = client.Store(filename);
    }
    else if (command == "fetch") {
        status = client.Fetch(filename);
    }
    else if (command == "delete") {
        status = client.Delete(filename);
    }
    else if (command == "list") {
        status = client.List();
    }
    else if (command == "stat") {
        status = client.Stat(filename);
    }

    if (!status.ok()) {
        std::cerr << "RPC failed: "
                  << status.error_code()
                  << " - "
                  << status.error_message()
                  << std::endl;

        return 1;
    }

    return 0;
}