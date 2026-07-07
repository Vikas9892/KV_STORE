include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(kv_config STATIC config.cpp)
target_include_directories(kv_config PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(kv_config PUBLIC nlohmann_json::nlohmann_json)
