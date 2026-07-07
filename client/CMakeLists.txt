add_executable(kv_client client.cpp)

add_library(kv_client_pool STATIC connection_pool.cpp)
target_include_directories(kv_client_pool PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(kv_client_pool PUBLIC Threads::Threads)
