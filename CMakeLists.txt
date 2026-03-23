cmake_minimum_required(VERSION 3.10)
project(ChatServer CXX)

# 配置编译选项
add_compile_options(-Wall -g)

# 配置最终的可执行文件路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# 头文件的搜索路径

# 加载子目录
add_subdirectory(src)