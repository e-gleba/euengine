cmake_minimum_required(VERSION 3.28)

project(
    egen
    VERSION 1.0.0
    DESCRIPTION "Edge-gen: SDL3 GPU engine with hot-reload architecture"
    LANGUAGES C CXX
)

option(USE_BASS_PROXY_LIB ON)

list(
    APPEND
    CMAKE_PREFIX_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake
    ${CMAKE_CURRENT_LIST_DIR}/cmake/code-quality
)

list(
    APPEND
    CMAKE_FIND_ROOT_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake
    ${CMAKE_CURRENT_LIST_DIR}/cmake/code-quality
)

message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

find_package(warnings CONFIG REQUIRED)

find_package(clang_format CONFIG REQUIRED)
find_package(clang_tidy CONFIG REQUIRED)
find_package(clang_doc CONFIG REQUIRED)

find_package(cpm CONFIG REQUIRED)

add_subdirectory(src)
