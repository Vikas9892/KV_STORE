add_library(kv_threadpool STATIC thread_pool.cpp)
target_include_directories(kv_threadpool PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(kv_threadpool PUBLIC Threads::Threads)
