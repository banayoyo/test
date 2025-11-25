# 测试可执行文件
add_executable(ut_proj ut_proj.cpp)

# 核心修改：直接链接原始目标 gtest/gtest_main，而非别名
target_link_libraries(ut_proj PRIVATE
    proj_logger
    front
    back
    gtest          # Googletest 原始核心目标（必存在）
    gtest_main     # Googletest 原始主函数目标（必存在）
    Threads::Threads # Linux/macOS 必需的线程库
)

# 头文件路径（不变）
target_include_directories(ut_proj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/proj/front
    ${CMAKE_SOURCE_DIR}/proj/back
    ${CMAKE_SOURCE_DIR}/proj/common
    ${CMAKE_SOURCE_DIR}/proj_logger
)

# 注册GTest测试（不变）
include(GoogleTest)
gtest_discover_tests(ut_proj)