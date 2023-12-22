if (BUILD_RUNTIME)
    project(acquire-video-runtime)

    add_subdirectory(src)
    add_subdirectory(tests)
endif ()