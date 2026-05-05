cmake_minimum_required(VERSION 3.16)
project(ChatServer CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 配置编译选项
add_compile_options(-Wall -g)

# 配置最终的可执行文件路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

find_package(Threads REQUIRED)

include(${CMAKE_SOURCE_DIR}/cmake/ProjectDependencies.cmake)

# 加载子目录
add_subdirectory(src)
