add_library(common
        STATIC
        utils/join.cpp
        utils/split.cpp
        socket/Socket.cpp
        socket/UnixSocket.cpp
        CommandManager.cpp
        Logger.cpp
)

target_link_libraries(common
        PRIVATE
        common_compile_flags
)

target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR}/include)
