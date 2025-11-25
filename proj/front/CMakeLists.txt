# 编译为动态库
add_library(front SHARED
    front.h
    front.cpp
)

# 链接日志核心库
target_link_libraries(front PRIVATE proj_logger)

# 头文件路径
target_include_directories(front PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_SOURCE_DIR}/proj_logger
)