add_library(proj_logger STATIC
    proj_logger.h
    proj_logger.cpp
)

# 关键修改：将 PRIVATE 改为 PUBLIC，让依赖 proj_logger 的目标能继承 spdlog 的头文件路径
target_link_libraries(proj_logger PUBLIC spdlog::spdlog)

# 头文件路径
target_include_directories(proj_logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../engine_base)