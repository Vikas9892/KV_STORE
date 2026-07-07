add_executable(kv_server main.cpp server.cpp client_session.cpp signal_handler.cpp)
target_include_directories(kv_server PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(kv_server PRIVATE kv_core kv_network kv_threadpool kv_config Threads::Threads)

add_executable(kv_server_epoll main_epoll.cpp epoll_server.cpp signal_handler.cpp)
target_include_directories(kv_server_epoll PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(kv_server_epoll PRIVATE kv_core kv_network kv_threadpool kv_config Threads::Threads)
