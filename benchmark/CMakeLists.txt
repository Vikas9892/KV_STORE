add_executable(kv_bench bench.cpp)
target_include_directories(kv_bench PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(kv_bench PRIVATE kv_core kv_network kv_threadpool Threads::Threads)

add_executable(kv_netbench netbench.cpp)
target_include_directories(kv_netbench PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(kv_netbench PRIVATE kv_client_pool Threads::Threads)
