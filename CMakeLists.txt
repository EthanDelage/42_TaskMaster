cmake_minimum_required(VERSION 3.30)
project(taskmaster)

set(CMAKE_CXX_STANDARD 14)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(include)

add_subdirectory(src/common)
add_subdirectory(src/server)
add_subdirectory(src/client)

add_compile_options(-Wall -Werror -Wextra)
