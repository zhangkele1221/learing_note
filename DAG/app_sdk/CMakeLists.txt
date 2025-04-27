# 创建静态库
add_library(app_sdk STATIC
    ModuleRegistry.cpp
    ModuleRegistry.h
)

# 链接依赖
target_link_libraries(app_sdk
    PUBLIC app_interface
    PUBLIC registry
)

# 包含路径
target_include_directories(app_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)




