cmake_minimum_required(VERSION 3.12)
project(dag_server)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 启用跨目录链接策略
cmake_policy(SET CMP0079 NEW)

# 查找依赖
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

# 添加子目录（确保顺序正确）
add_subdirectory(registry)
add_subdirectory(app_interface)
add_subdirectory(app_sdk)
add_subdirectory(echo_server)  # 必须先于 sw/server，确保 echo_lib 已定义
add_subdirectory(sw/server)