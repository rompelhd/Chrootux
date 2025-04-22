cmake_minimum_required(VERSION 3.10)
project(Chrootux)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "-g -Iinclude")
set(CMAKE_CXX_FLAGS "-g -Iinclude")

file(GLOB_RECURSE CPP_FILES src/*.cpp)
file(GLOB_RECURSE C_FILES lib/*.c)

foreach(C_FILE ${C_FILES})
    get_filename_component(C_FILE_NAME ${C_FILE} NAME_WE)
    add_library(${C_FILE_NAME} SHARED ${C_FILE})
    target_include_directories(${C_FILE_NAME} PRIVATE include)
endforeach()

add_executable(${PROJECT_NAME} ${CPP_FILES})

target_link_libraries(${PROJECT_NAME} archive ncurses curl)
