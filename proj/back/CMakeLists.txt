# 编译为动态库
add_library(back SHARED
    back.h
    back.cpp
)

# 链接日志核心库
target_link_libraries(back PRIVATE proj_logger)

# 头文件路径
target_include_directories(back PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_SOURCE_DIR}/proj_logger
)