add_executable(server
        main.cpp
        Process.cpp
        Taskmaster.cpp
        TaskManager.cpp
        ConfigParser.cpp
        UnixSocketServer.cpp
        ClientSession.cpp
        ProcessGroup.cpp
        ProcessPool.cpp
)

include(FetchContent)

# Disable yaml-cpp tools/tests/examples
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)

FetchContent_MakeAvailable(yaml-cpp)

target_compile_options(server
  PRIVATE
  ${CXX_FLAGS}
)

target_link_libraries(server
  PRIVATE
  common
  yaml-cpp::yaml-cpp
)

