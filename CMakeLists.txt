cmake_minimum_required(VERSION 3.22.1)
project(taskmaster)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CXX_FLAGS -Wall -Werror -Wextra)

include_directories(include)

add_subdirectory(src/common)
add_subdirectory(src/server)
add_subdirectory(src/client)

