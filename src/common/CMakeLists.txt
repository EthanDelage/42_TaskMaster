add_library(common
  STATIC
  utils.cpp
  split.cpp
)

target_compile_options(common
  PRIVATE
  ${CMAKE_CXX_ERROR_FLAGS}
)

target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR}/include)
