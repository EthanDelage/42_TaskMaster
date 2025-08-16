add_executable(client
  main.cpp
)

target_compile_options(client
  PRIVATE
  ${CMAKE_CXX_ERROR_FLAGS}
)

target_link_libraries(client common)
