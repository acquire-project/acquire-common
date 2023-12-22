if (BUILD_COMMON_DRIVER)
    project(acquire-driver-common)

    add_subdirectory(src)
    add_subdirectory(tests)
endif ()