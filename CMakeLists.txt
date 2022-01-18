cmake_minimum_required(VERSION 3.10)

project(FluxHarmonic C)

find_package(PkgConfig REQUIRED)

#########################################
# flux-compose executable

add_executable(flux-compose src/core/main.c src/core/repl.c)
target_link_libraries(flux-compose PUBLIC flux)

#########################################
# flux shared library
find_package(Threads REQUIRED)

# We could also use find_package(SDL), but that's only CMake 3.19
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2-Gfx REQUIRED IMPORTED_TARGET SDL2_gfx)
pkg_check_modules(SDL2-Image REQUIRED IMPORTED_TARGET SDL2_image)

add_library(flux SHARED lib/flux.h lib/lib.c lib/log.c lib/scene.c lib/mem.c lib/file.c lib/script.c lib/vector.c)
target_link_libraries(flux PkgConfig::SDL2 PkgConfig::SDL2-Gfx PkgConfig::SDL2-Image Threads::Threads)

#########################################
# run-tests executable
add_executable(run-tests test/test-main.c test/test-lang.c test/test-vector.c)
target_link_libraries(run-tests PUBLIC flux PkgConfig::SDL2 PkgConfig::SDL2-Gfx PkgConfig::SDL2-Image Threads::Threads)
