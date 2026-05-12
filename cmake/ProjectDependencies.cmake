include(FetchContent)

set(CHATSERVER_DEPS_DIR "${CMAKE_SOURCE_DIR}/.deps" CACHE PATH
    "Directory used for locally managed third-party dependencies.")
set(FETCHCONTENT_BASE_DIR "${CHATSERVER_DEPS_DIR}/fetchcontent" CACHE PATH
    "Persistent cache for FetchContent dependencies.")

if (NOT DEFINED MUDUO_ROOT OR MUDUO_ROOT STREQUAL "")
    if (DEFINED ENV{MUDUO_ROOT} AND NOT "$ENV{MUDUO_ROOT}" STREQUAL "")
        set(MUDUO_ROOT "$ENV{MUDUO_ROOT}" CACHE PATH "Muduo installation prefix.")
    else()
        set(MUDUO_ROOT "/usr/local" CACHE PATH
            "Muduo installation prefix.")
    endif()
endif()

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

find_path(CHATSERVER_MUDUO_INCLUDE_DIR
    NAMES muduo/net/EventLoop.h
    HINTS
        "${MUDUO_ROOT}/include"
    NO_DEFAULT_PATH
)

find_library(CHATSERVER_MUDUO_NET_LIBRARY
    NAMES muduo_net
    HINTS
        "${MUDUO_ROOT}/lib"
        "${MUDUO_ROOT}/lib64"
    NO_DEFAULT_PATH
)

find_library(CHATSERVER_MUDUO_BASE_LIBRARY
    NAMES muduo_base
    HINTS
        "${MUDUO_ROOT}/lib"
        "${MUDUO_ROOT}/lib64"
    NO_DEFAULT_PATH
)

if (NOT CHATSERVER_MUDUO_INCLUDE_DIR OR
    NOT CHATSERVER_MUDUO_NET_LIBRARY OR
    NOT CHATSERVER_MUDUO_BASE_LIBRARY)
    message(FATAL_ERROR
        "Muduo was not found under ${MUDUO_ROOT}. "
        "For mode A, install muduo locally (recommended: /usr/local) or set MUDUO_ROOT explicitly.")
endif()

get_filename_component(CHATSERVER_MUDUO_LIB_DIR
    "${CHATSERVER_MUDUO_NET_LIBRARY}" DIRECTORY)

add_library(chatserver_muduo INTERFACE)
target_include_directories(chatserver_muduo INTERFACE
    "${CHATSERVER_MUDUO_INCLUDE_DIR}")
target_link_libraries(chatserver_muduo INTERFACE
    "${CHATSERVER_MUDUO_NET_LIBRARY}"
    "${CHATSERVER_MUDUO_BASE_LIBRARY}")
