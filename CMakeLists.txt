cmake_minimum_required(VERSION 3.16)

project(FluxHarmonic C)

include("cmake/compiler.cmake")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

# We could also use find_package(SDL), but that's only CMake 3.19
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_GFX REQUIRED IMPORTED_TARGET SDL2_gfx)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)

include_directories(lib src ${SDL2_INCLUDE_DIRS} ${SDL2_GFX_INCLUDE_DIRS}
  ${SDL2_IMAGE_INCLUDE_DIRS})

add_library(flux)
target_link_libraries(flux PkgConfig::SDL2 PkgConfig::SDL2_GFX
  PkgConfig::SDL2_IMAGE Threads::Threads glfw OpenGL::GL)

add_executable(flux-compose)
target_link_libraries(flux-compose flux)

add_executable(run-tests)
target_link_libraries(run-tests PUBLIC flux)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
