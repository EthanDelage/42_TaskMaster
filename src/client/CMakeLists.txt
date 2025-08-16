add_executable(client
        main.cpp
)

target_compile_options(client PRIVATE -Wall -Werror -Wextra)

target_link_libraries(client common)
