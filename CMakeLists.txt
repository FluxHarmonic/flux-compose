cmake_minimum_required(VERSION 3.16)

project(FluxHarmonic C)

include("cmake/compiler.cmake")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(lib src)

add_library(flux)
target_link_libraries(flux Threads::Threads glfw OpenGL::GL)

add_executable(flux-compose)
target_link_libraries(flux-compose flux)

add_executable(run-tests)
target_link_libraries(run-tests PUBLIC flux)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
