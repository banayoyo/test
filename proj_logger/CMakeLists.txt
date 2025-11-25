# 编译为静态库（日志核心模块，静态链接更方便）
add_library(proj_logger STATIC
    proj_logger.h
    proj_logger.cpp
)

# 链接spdlog
target_link_libraries(proj_logger PRIVATE spdlog::spdlog)

# 头文件路径
target_include_directories(proj_logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})