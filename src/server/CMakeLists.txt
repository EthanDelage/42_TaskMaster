add_executable(taskmasterd
        main.cpp
        Process.cpp
        Taskmaster.cpp
        TaskManager.cpp
        ConfigParser.cpp
        UnixSocketServer.cpp
        ClientSession.cpp
        ProcessGroup.cpp
        ProcessPool.cpp
        PollFds.cpp
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

target_link_libraries(taskmasterd
        PRIVATE
        common_compile_flags
        common
        yaml-cpp::yaml-cpp
)

