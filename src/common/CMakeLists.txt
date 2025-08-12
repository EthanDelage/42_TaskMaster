add_library(common STATIC
        utils.cpp
        split.cpp
)

target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR}/include)