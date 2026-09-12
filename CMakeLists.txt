cmake_minimum_required(VERSION 3.25)
project(S7EmbeddedDriver LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp
    src/adapters/s7adapter.cpp
    include/snap7micro/snap_msgsock.cpp
    include/snap7micro/snap_sysutils.cpp
    include/snap7micro/s7_micro_client.cpp
    include/snap7micro/s7_isotcp.cpp
    include/snap7micro/s7_micro_client.cpp
    include/snap7micro/s7_peer.cpp
    include/nlohmann/json.hpp
    include/nlohmann/json_fwd.hpp
)

add_executable(S7EmbeddedDriver ${SOURCES})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(S7EmbeddedDriver PRIVATE
        # -fno-exceptions
        # -fno-enforce-eh-specs
        -fno-rtti
        -Wall -Wextra -Wpedantic
        -O2
    )
endif()

if(NOT EXISTS "${CMAKE_BINARY_DIR}/../compile_commands.json")
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink 
        "${CMAKE_BINARY_DIR}/compile_commands.json" 
        "${CMAKE_BINARY_DIR}/../compile_commands.json"
    )
endif()

target_include_directories(S7EmbeddedDriver 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)