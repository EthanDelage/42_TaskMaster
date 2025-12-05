add_executable(taskmasterctl
        main.cpp
        TaskmasterCtl.cpp
        UnixSocketClient.cpp
)

target_compile_options(taskmasterctl
        PRIVATE
        ${CXX_FLAGS}
)

target_link_libraries(taskmasterctl PRIVATE common)

if(APPLE)
    find_library(EDIT_LIB edit REQUIRED)
    target_link_libraries(taskmasterctl PRIVATE ${EDIT_LIB})
else()
    find_path(READLINE_INCLUDE_DIR readline/readline.h)
    find_library(READLINE_LIBRARY readline)

    if(NOT READLINE_INCLUDE_DIR OR NOT READLINE_LIBRARY)
        message(FATAL_ERROR "Could not find GNU readline")
    endif()

    target_include_directories(taskmasterctl PRIVATE ${READLINE_INCLUDE_DIR})
    target_link_libraries(taskmasterctl PRIVATE ${READLINE_LIBRARY})
endif()
