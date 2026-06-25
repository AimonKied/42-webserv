cmake_minimum_required(VERSION 3.15)
project(webserv VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${PROJECT_SOURCE_DIR}/include)

file(GLOB_RECURSE WEB_SERV_SOURCES
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)

add_executable(webserv ${WEB_SERV_SOURCES})

target_include_directories(webserv PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_compile_options(webserv PRIVATE -Wall -Wextra -Werror)
