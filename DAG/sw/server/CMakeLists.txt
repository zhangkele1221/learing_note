# sw/server/CMakeLists.txt


# 添加第三方库路径
include_directories(/home/learing/incubator-brpc/output/include)
link_directories(/home/learing/incubator-brpc/output/lib)

# 定义可执行文件（包含所有必要源文件）
add_executable(dag_server
    ${PROTO_SRCS}
    # 主程序入口
    main.cpp
    ServerMain.cpp

    # 核心组件实现
    AppLoader.cpp
    AppProxy.cpp
    ModuleTopologyManager.cpp
    RpcServer.cpp
    RpcServiceImpl.cpp
    SearchWorker.cpp
    ServerConfiguration.cpp
    ChannelManager.cpp
    ModuleScheduler.cpp

    # 子目录中的公共组件
    ComponentRegistry.cpp
    RpcComponentImpl.cpp

    # Protobuf 生成的代码（假设已在 sw/proto 中生成）
    topology_config.pb.cc
)

# 包含头文件路径
target_include_directories(dag_server INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}      # 当前目录（sw/server）
    ${CMAKE_CURRENT_BINARY_DIR}/../proto  # Protobuf 生成的头文件路径
    ${Protobuf_INCLUDE_DIRS}         # Protobuf 系统头文件
)


add_dependencies(dag_server echo_lib)

# 链接依赖库
target_link_libraries(dag_server 
    echo_lib  # 放在最前面
    #${CMAKE_CURRENT_SOURCE_DIR}/../echo_server/libecho_lib.a
    app_sdk          # 依赖 app_sdk 库
    protobuf::libprotobuf
    brpc
    app_interface  # 新增
    protobuf
    ssl
    crypto
    leveldb
    gflags
    pthread
    Threads::Threads
    #/home/learing/learing_note/DAG/build/echo_server/libecho_lib.a
    ${CMAKE_DL_LIBS}
)

