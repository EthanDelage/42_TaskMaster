add_library(common
  STATIC
  utils/join.cpp
  utils/split.cpp
  UnixSocket.cpp
  CommandManager.cpp
)

target_compile_options(common
  PRIVATE
  ${CXX_FLAGS}
)

target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR}/include)
