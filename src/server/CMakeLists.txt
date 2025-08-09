add_executable(server
        main.cpp
        config/Config.cpp
        config/ProgramConfig.cpp
)

include(FetchContent)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)

FetchContent_MakeAvailable(yaml-cpp)

target_link_libraries(server PRIVATE common yaml-cpp::yaml-cpp)

