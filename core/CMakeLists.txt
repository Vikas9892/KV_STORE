add_library(kv_core STATIC kv_store.cpp wal.cpp)
target_include_directories(kv_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(kv_core PUBLIC Threads::Threads)
