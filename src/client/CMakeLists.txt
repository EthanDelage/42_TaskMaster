add_executable(taskmasterctl
        main.cpp
        Client.cpp
)

target_compile_options(taskmasterctl
  PRIVATE
  ${CXX_FLAGS}
)

target_link_libraries(taskmasterctl common)
