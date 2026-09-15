cmake_minimum_required(VERSION 3.10)
project(AMLua C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# Include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/jni
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/aml
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/lua
)

# Lua 5.4 core & library sources
set(LUA_SOURCES
    jni/lua/lapi.c
    jni/lua/lauxlib.c
    jni/lua/lbaselib.c
    jni/lua/lcode.c
    jni/lua/lcorolib.c
    jni/lua/lctype.c
    jni/lua/ldblib.c
    jni/lua/ldebug.c
    jni/lua/ldo.c
    jni/lua/ldump.c
    jni/lua/lfunc.c
    jni/lua/lgc.c
    jni/lua/linit.c
    jni/lua/liolib.c
    jni/lua/llex.c
    jni/lua/lmathlib.c
    jni/lua/lmem.c
    jni/lua/loadlib.c
    jni/lua/lobject.c
    jni/lua/lopcodes.c
    jni/lua/loslib.c
    jni/lua/lparser.c
    jni/lua/lstate.c
    jni/lua/lstring.c
    jni/lua/lstrlib.c
    jni/lua/ltable.c
    jni/lua/ltablib.c
    jni/lua/ltm.c
    jni/lua/lundump.c
    jni/lua/lutf8lib.c
    jni/lua/lvm.c
    jni/lua/lzio.c
)

# AMLua plugin sources
set(AMLUA_SOURCES
    jni/main.cpp
    jni/lua_bindings.cpp
    jni/crash_handler.cpp
    jni/http_client.cpp
    jni/json_helper.cpp
    jni/aml/logger.cpp
    jni/aml/config.cpp
    ${LUA_SOURCES}
)

# Define shared library output: libAMLua64.so or libAMLua32.so
add_library(AMLua SHARED ${AMLUA_SOURCES})

if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(AMLUA_LIB_NAME "AMLua64")
else()
    set(AMLUA_LIB_NAME "AMLua32")
endif()

set_target_properties(AMLua PROPERTIES
    OUTPUT_NAME "${AMLUA_LIB_NAME}"
    PREFIX "lib"
    SUFFIX ".so"
)

# Compile options
target_compile_definitions(AMLua PRIVATE
    LUA_COMPAT_5_3
    LUA_USE_POSIX
)

target_compile_options(AMLua PRIVATE
    -O3
    -fPIC
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -fvisibility=hidden
)

# Target link libraries
target_link_libraries(AMLua PRIVATE
    log
    dl
    m
)

# Strip symbols for optimal size
target_link_options(AMLua PRIVATE
    -s
)
