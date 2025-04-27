# 生成Proto文件（假设使用protobuf）
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
    EchoService.proto
)

# 创建可执行文件
add_executable(echo_server
    EchoApp.cpp
    EchoApp.h
    EchoModule.cpp
    EchoModule.h
    ${PROTO_SRCS} ${PROTO_HDRS}
)

# 链接依赖
target_link_libraries(echo_server
    PRIVATE app_interface app_sdk
    PRIVATE protobuf::libprotobuf  # 如果使用protobuf
)

# 包含路径
target_include_directories(echo_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
)