add_library(kv_core STATIC kv_store.cpp)
target_include_directories(kv_core PUBLIC ${PROJECT_SOURCE_DIR})
