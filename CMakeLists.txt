cmake_minimum_required(VERSION 3.16)

project(FluxHarmonic C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -ggdb -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -flto")
set(CMAKE_INTERPROCEDURAL_OPTMIZATION TRUE)

add_compile_options(-Wdeclaration-after-statement)
add_compile_options(-Wduplicated-branches)
add_compile_options(-Wduplicated-cond)
add_compile_options(-Wempty-body)
add_compile_options(-Werror-implicit-function-declaration)
add_compile_options(-Wextra)
add_compile_options(-Wignored-qualifiers)
add_compile_options(-Wimplicit-fallthrough)
add_compile_options(-Wmisleading-indentation)
add_compile_options(-Wno-conversion)
add_compile_options(-Wno-sign-compare)
add_compile_options(-Wno-sign-conversion)
add_compile_options(-Wstrict-prototypes)
add_compile_options(-Wuninitialized)
add_compile_options(-Wunknown-pragmas)
add_compile_options(-Wunused-but-set-variable)
add_compile_options(-Wunused-function)
add_compile_options(-Wunused-label)
add_compile_options(-Wunused-result)
add_compile_options(-Wunused-value)
add_compile_options(-Wunused-variable)
add_compile_options(-fno-delete-null-pointer-checks)
add_compile_options(-fno-strict-overflow)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# We could also use find_package(SDL), but that's only CMake 3.19
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_GFX REQUIRED IMPORTED_TARGET SDL2_gfx)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)

include_directories(lib src ${SDL2_INCLUDE_DIRS} ${SDL2_GFX_INCLUDE_DIRS}
  ${SDL2_IMAGE_INCLUDE_DIRS})

add_library(flux)
target_link_libraries(flux PkgConfig::SDL2 PkgConfig::SDL2_GFX
  PkgConfig::SDL2_IMAGE Threads::Threads)

add_executable(flux-compose)
target_link_libraries(flux-compose flux)

add_executable(run-tests)
target_link_libraries(run-tests PUBLIC flux)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
