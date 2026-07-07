add_library(kv_network STATIC socket.cpp protocol.cpp)
target_include_directories(kv_network PUBLIC ${PROJECT_SOURCE_DIR})
