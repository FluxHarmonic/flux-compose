cmake_minimum_required(VERSION 3.10)

project(FluxHarmonic C)

#########################################
# flux-compose executable
find_package(PkgConfig REQUIRED)
pkg_check_modules(Guile3 REQUIRED IMPORTED_TARGET guile-3.0)

add_executable(flux-compose src/core/main.c)
target_link_libraries(flux-compose PkgConfig::Guile3)

#########################################
# flux shared library
find_package(Threads REQUIRED)

# We could also use find_package(SDL), but that's only CMake 3.19
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2-Gfx REQUIRED IMPORTED_TARGET SDL2_gfx)
pkg_check_modules(SDL2-Image REQUIRED IMPORTED_TARGET SDL2_image)

add_library(flux SHARED lib/flux.h lib/lib.c lib/log.c lib/scene.c)
target_link_libraries(flux PkgConfig::SDL2 PkgConfig::SDL2-Gfx PkgConfig::SDL2-Image Threads::Threads)
