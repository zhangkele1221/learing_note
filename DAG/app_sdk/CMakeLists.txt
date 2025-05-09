add_library(app_sdk STATIC
    ModuleRegistry.cpp   # 必须包含源文件
)

target_link_libraries(app_sdk PUBLIC
    app_interface
    registry
)

target_include_directories(app_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
