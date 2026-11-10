cmake_minimum_required(VERSION 3.20)

project(JARVIS VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(jarvis
    src/main.cpp
    src/core/Assistant.cpp
    src/ai/CloudClient.cpp
    src/tools/ToolManager.cpp
)

target_include_directories(jarvis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(jarvis PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

target_compile_options(jarvis PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

