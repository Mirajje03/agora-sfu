cmake_minimum_required(VERSION 3.16)
project(sfu_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libdatachannel (ensure you installed it or set CMAKE_PREFIX_PATH accordingly)
find_package(libdatachannel REQUIRED)

# Fetch nlohmann_json (single-header) for simple signaling messages
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(sfu_server main.cpp)

target_include_directories(sfu_server PRIVATE libdatachannel/include)

target_link_libraries(sfu_server
  PRIVATE
    libdatachannel
    nlohmann_json::nlohmann_json
)
