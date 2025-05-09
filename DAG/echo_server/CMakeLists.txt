cmake_minimum_required(VERSION 3.12)
project(echo_server)

find_package(Protobuf REQUIRED)


protobuf_generate_cpp(
    PROTO_SRCS PROTO_HDRS
    ${CMAKE_CURRENT_SOURCE_DIR}/EchoService.proto  # 完整路径指定输入文件
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}   # 输出到构建目录
)


execute_process(
    COMMAND protoc
        -I=${CMAKE_CURRENT_SOURCE_DIR}
        --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/EchoService.proto
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 手动指定生成的文件路径
set(PROTO_SRCS ${CMAKE_CURRENT_BINARY_DIR}/EchoService.pb.cc)
set(PROTO_HDRS ${CMAKE_CURRENT_BINARY_DIR}/EchoService.pb.h)

# 创建静态库
add_library(echo_lib STATIC
    EchoApp.cpp
    EchoModule.cpp
    ${PROTO_SRCS}
)

# 包含路径
target_include_directories(echo_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)



# 链接依赖
target_link_libraries(echo_lib
    PRIVATE
    app_interface
    app_sdk
    protobuf::libprotobuf
    brpc  # 如果使用 BRPC 的 RPC 服务
)

# 统一库输出路径
set_target_properties(echo_lib PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)