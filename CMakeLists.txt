cmake_minimum_required(VERSION 3.15)
project(proj_log_system LANGUAGES CXX)

# 全局编译选项
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===================== 关键：链接线程库（GTest必需）=====================
find_package(Threads REQUIRED)

# ===================== 编译spdlog（不变）=====================
add_subdirectory(third_party/spdlog EXCLUDE_FROM_ALL)

# ===================== 重新配置GTest编译选项（核心修改）=====================
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)  # 强制编译GTest核心库
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)

# ===================== 工程模块编译（不变）=====================
add_subdirectory(proj_logger)
add_subdirectory(proj)
add_subdirectory(test)