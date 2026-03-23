set(
    SRC_FILES
    main.cpp
    chatserver.cpp
    chatservice.cpp
    db/db.cpp
    model/usermodel.cpp
    model/offlinemessagemodel.cpp
    model/friendmodel.cpp
    model/groupmodel.cpp
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
)

# 指定可执行文件以依赖的库文件
target_link_libraries(ChatServer PRIVATE mymuduo mysqlclient pthread hiredis)