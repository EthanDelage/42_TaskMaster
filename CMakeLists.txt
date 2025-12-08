cmake_minimum_required(VERSION 3.22.1)
project(taskmaster)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(common_compile_flags INTERFACE)
target_compile_options(common_compile_flags INTERFACE
        -Wall
        -Werror
        -Wextra
        $<$<CONFIG:Debug>:-Wno-unused>
)
target_compile_definitions(common_compile_flags INTERFACE
        $<$<CONFIG:Debug>:DEBUG>
)

include_directories(include)

add_subdirectory(src/common)
add_subdirectory(src/server)
add_subdirectory(src/client)

