cmake_minimum_required(VERSION 3.26)
project(c_json C)

set(CMAKE_C_STANDARD 11)

add_library(cjson c_json.c)

add_executable(main main.c)
target_link_libraries(main cjson)
