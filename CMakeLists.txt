cmake_minimum_required(VERSION 3.20)

project(kv_store
    VERSION 0.1.0
    DESCRIPTION "High-Performance Concurrent In-Memory Key-Value Store"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)
add_compile_options("$<$<CONFIG:Debug>:-g;-O0;-fsanitize=address,undefined>")
add_compile_options("$<$<CONFIG:Release>:-O3;-march=native>")
add_link_options("$<$<CONFIG:Debug>:-fsanitize=address,undefined>")

find_package(Threads REQUIRED)

add_subdirectory(core)
add_subdirectory(network)
add_subdirectory(threadpool)
add_subdirectory(server)
