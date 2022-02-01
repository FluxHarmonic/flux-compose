cmake_minimum_required(VERSION 3.16)

project(FluxHarmonic C)

include("cmake/compiler.cmake")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Freetype REQUIRED)

include_directories(lib src lib/vendor/spng lib/vendor/miniz)

add_library(flux)
target_link_libraries(flux m z ${FREETYPE_LIBRARIES} Threads::Threads glfw OpenGL::GL)
target_include_directories(flux PRIVATE ${FREETYPE_INCLUDE_DIRS})

add_executable(flux-compose)
target_link_libraries(flux-compose flux)

add_executable(run-tests)
target_link_libraries(run-tests PUBLIC flux)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
