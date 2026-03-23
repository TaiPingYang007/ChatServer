set(SRC_FILES main.cpp)

#生成可执行文件
add_executable(ChatClient ${SRC_FILES})

#指定可执行文件以依赖的库文件
target_link_libraries(ChatClient pthread)