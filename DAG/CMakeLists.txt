cmake_minimum_required(VERSION 3.12)
project(dag_server)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 在链接之前添加 find_package
find_package(Protobuf REQUIRED)

# 添加项目头文件路径
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/sw
    ${CMAKE_SOURCE_DIR}/sw/server
    ${CMAKE_SOURCE_DIR}/sw/server/component
    ${CMAKE_SOURCE_DIR}/sw/common
    ${CMAKE_SOURCE_DIR}/app_interface
    ${CMAKE_SOURCE_DIR}/registry       # 添加 registry 模块路径
    ${CMAKE_SOURCE_DIR}/cppcommon  # 第三方库路径
)

# 添加第三方库路径（如 brpc）
include_directories(/home/learing/incubator-brpc/output/include)
link_directories(/home/learing/incubator-brpc/output/lib)

# 生成可执行文件
add_executable(dag_server
    sw/server/main.cpp
    sw/server/ServerMain.cpp
    sw/server/AppLoader.cpp
    sw/server/AppProxy.cpp
    sw/server/ModuleTopologyManager.cpp
    sw/server/RpcServer.cpp
    sw/server/RpcServiceImpl.cpp
    sw/server/SearchWorker.cpp
    sw/server/common/ComponentRegistry.cpp
    sw/server/component/RpcComponentImpl.cpp
)


# 链接时使用正确的目标名称
target_link_libraries(dag_server
    PRIVATE 
    protobuf::libprotobuf  # 正确目标名称
    brpc
    Threads::Threads
    ${CMAKE_DL_LIBS}
)