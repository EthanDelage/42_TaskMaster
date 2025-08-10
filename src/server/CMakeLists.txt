add_executable(server
        main.cpp
        Process.cpp
        ${PROJECT_SOURCE_DIR}/include/server/Process.hpp
)

target_link_libraries(server common)
