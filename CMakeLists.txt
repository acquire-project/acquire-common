cmake_minimum_required(VERSION 3.23)
project(acquire-common)
cmake_policy(SET CMP0079 NEW) # use targets from other directories
enable_testing()

include(cmake/aq_require.cmake)
include(cmake/components.cmake)
include(cmake/git-versioning.cmake)
include(cmake/ide.cmake)
include(cmake/msvc.cmake)
include(cmake/simd.cmake)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(acquire-core-libs)
add_subdirectory(acquire-video-runtime)
add_subdirectory(acquire-driver-common)

include(CPack)