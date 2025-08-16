add_executable(client
  main.cpp
)

target_compile_options(client
  PRIVATE
  ${CXX_FLAGS}
)

target_link_libraries(client common)
