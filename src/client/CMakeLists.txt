add_executable(taskmasterctl
        main.cpp
        Client.cpp
)

target_link_libraries(taskmasterctl common)