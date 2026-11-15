cmake_minimum_required(VERSION 3.16)

project(DFS_custom LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

add_library(dfs_proto
    generated/dfs_service.pb.cc
    generated/dfs_service.grpc.pb.cc
)

target_include_directories(dfs_proto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
)

target_link_libraries(dfs_proto PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
)

add_executable(dfs_server
    server/main.cpp
    server/FileStorage.cpp
    server/FileServiceImpl.cpp
)

target_include_directories(dfs_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server
)

target_link_libraries(dfs_server PRIVATE
    dfs_proto
)

add_executable(dfs_client
    client/main.cpp
    client/DFSClient.cpp
)

target_include_directories(dfs_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/client
)

target_link_libraries(dfs_client PRIVATE
    dfs_proto
)