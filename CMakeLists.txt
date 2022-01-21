cmake_minimum_required(VERSION 3.16)

project(FluxHarmonic C)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# We could also use find_package(SDL), but that's only CMake 3.19
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2-Gfx REQUIRED IMPORTED_TARGET SDL2_gfx)
pkg_check_modules(SDL2-Image REQUIRED IMPORTED_TARGET SDL2_image)

include_directories(lib src)

add_library(flux)
target_link_libraries(flux PkgConfig::SDL2 PkgConfig::SDL2-Gfx
  PkgConfig::SDL2-Image Threads::Threads)

add_executable(flux-compose)
target_link_libraries(flux-compose flux)

add_executable(run-tests)
target_link_libraries(run-tests PUBLIC flux)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
