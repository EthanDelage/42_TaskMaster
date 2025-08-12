add_executable(client
        main.cpp
        Client.cpp
)

target_link_libraries(client common)