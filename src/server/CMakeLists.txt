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

# 头文件挂载
target_include_directories(ChatServer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/server
    ${CMAKE_SOURCE_DIR}/include/server/model
    ${CMAKE_SOURCE_DIR}/include/server/db
    ${CMAKE_SOURCE_DIR}/include/server/redis
    /home/taipingyang/thirdparty/muduo/include
)

target_compile_definitions(ChatServer PRIVATE
    CHATSERVER_LOG_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

set(MUDUO_LIB_DIR /home/taipingyang/thirdparty/muduo/lib)

# 指定可执行文件以依赖的库文件（静态库，无需 RPATH）
target_link_libraries(ChatServer PRIVATE
    ${MUDUO_LIB_DIR}/libmuduo_net.a
    ${MUDUO_LIB_DIR}/libmuduo_base.a
    mysqlclient pthread hiredis)
