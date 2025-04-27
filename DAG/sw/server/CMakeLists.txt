# 创建可执行文件
add_executable(dag_server
    main.cpp
    AppLoader.cpp
    AppLoader.h
    AppProxy.cpp
    AppProxy.h
    ModuleTopologyManager.cpp
    ModuleTopologyManager.h
    RpcServer.cpp
    RpcServer.h
    RpcServiceImpl.cpp
    RpcServiceImpl.h
    SearchWorker.cpp
    SearchWorker.h
    ServerMain.cpp
    # 子目录内容
    common/ComponentRegistry.cpp
    common/ComponentRegistry.h
    component/RpcComponentImpl.cpp
    component/RpcComponentImpl.h
)

# 链接依赖
target_link_libraries(dag_server
    PRIVATE app_interface app_sdk registry
    PRIVATE protobuf::libprotobuf  # 如果使用protobuf
)

# 包含路径
target_include_directories(dag_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/component
)