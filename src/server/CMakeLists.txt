set(
    SRC_FILES
    main.cpp
    logger.cpp
    chatserver.cpp
    chatservice.cpp
    db/db.cpp
    model/usermodel.cpp
    model/offlinemessagemodel.cpp
    model/friendmodel.cpp
    model/friendrequestmodel.cpp
    model/groupmodel.cpp
    model/grouprequestmodel.cpp
    redis/redis.cpp
)

# 生成可执行文件
add_executable(ChatServer ${SRC_FILES})

# 头文件挂载（muduo 已装到 /usr/local/include，无需单独指定）
target_include_directories(ChatServer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/server
    ${CMAKE_SOURCE_DIR}/include/server/model
    ${CMAKE_SOURCE_DIR}/include/server/db
    ${CMAKE_SOURCE_DIR}/include/server/redis
)

target_compile_definitions(ChatServer PRIVATE
    CHATSERVER_LOG_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# 指定可执行文件依赖的库文件（muduo 已装到 /usr/local/lib，直接用库名）
target_link_libraries(ChatServer PRIVATE
    muduo_net muduo_base
    mysqlclient pthread hiredis)
