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

find_library(CHATSERVER_MYSQLCLIENT_LIBRARY mysqlclient)
if (NOT CHATSERVER_MYSQLCLIENT_LIBRARY)
    message(FATAL_ERROR "mysqlclient library not found. Install default-libmysqlclient-dev.")
endif()

find_library(CHATSERVER_HIREDIS_LIBRARY hiredis)
if (NOT CHATSERVER_HIREDIS_LIBRARY)
    message(FATAL_ERROR "hiredis library not found. Install libhiredis-dev.")
endif()

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

set_target_properties(ChatServer PROPERTIES
    BUILD_RPATH "${CHATSERVER_MUDUO_LIB_DIR}"
)

target_link_libraries(ChatServer PRIVATE
    chatserver_muduo
    nlohmann_json::nlohmann_json
    ${CHATSERVER_MYSQLCLIENT_LIBRARY}
    Threads::Threads
    ${CHATSERVER_HIREDIS_LIBRARY})
