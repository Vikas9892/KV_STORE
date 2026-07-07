include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(kv_tests
    test_kv_store.cpp
    test_protocol.cpp
    test_thread_pool.cpp
)

target_include_directories(kv_tests PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(kv_tests PRIVATE
    kv_core
    kv_network
    kv_threadpool
    GTest::gtest_main
    Threads::Threads
)

gtest_discover_tests(kv_tests)
