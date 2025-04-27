# 创建接口库（纯头文件）
add_library(app_interface INTERFACE)

# 添加头文件路径
target_include_directories(app_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)