# registry/CMakeLists.txt
add_library(registry STATIC
    learing_searcher_work.cpp
    element_registry.h
    factory_registry.h
    registry.h
    registry_inl.h
)

# 添加头文件路径并传递给依赖者
target_include_directories(registry PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}  # 导出 registry 目录路径
)

