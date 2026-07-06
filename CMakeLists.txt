cmake_minimum_required(VERSION 3.20)

project(Chapter16_MultithreadedDownloader)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

add_executable(Chapter16_MultithreadedDownloader
    src/main.cpp
)

target_link_libraries(Chapter16_MultithreadedDownloader
    PRIVATE
    CURL::libcurl
)
