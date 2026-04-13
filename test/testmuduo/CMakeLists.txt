# 1. 强制 CMake 最低版本
cmake_minimum_required(VERSION 3.22)

# 2. 指定项目名称，并强制要求 C++17 标准 (工业级强制规范)
project(ChatServer CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 3. Modern CMake 做法：使用独立指令添加编译选项，抛弃 set 拼接！
add_compile_options(-g -Wall)

# 4. 定义源文件 (不要加前面的 ./，容易引发路径强迫症问题)
set(SRC_LIST 
    ./muduo_server.cpp
)

# 5. 设置可执行文件最终存储路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 6. 生成可执行文件
add_executable(server ${SRC_LIST})

# 7. 链接依赖库
set(MUDUO_LIB_DIR /home/taipingyang/thirdparty/muduo/lib)

target_include_directories(server PRIVATE
    /home/taipingyang/thirdparty/muduo/include
)

target_link_libraries(server
    PRIVATE
    ${MUDUO_LIB_DIR}/libmuduo_net.a
    ${MUDUO_LIB_DIR}/libmuduo_base.a
    pthread
)
