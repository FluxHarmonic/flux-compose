include(CheckCCompilerFlag)

option(FLUX_USE_ASAN "Use libASAN in the compiled library and binary" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -ggdb -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -flto")
set(CMAKE_INTERPROCEDURAL_OPTMIZATION TRUE)

function(add_compile_option_if option variable)
  check_c_compiler_flag(${option} ${variable})

  if (${${variable}})
    add_compile_options(${option})
  endif()

  unset(${variable} CACHE)
endfunction()

add_compile_option_if(-Wdeclaration-after-statement
  WDECLARATION-AFTER-STATEMENT)
add_compile_option_if(-Wduplicated-branches WDUPLICATED-BRANCHES)
add_compile_option_if(-Wduplicated-cond WDUPLICATED-COND)
add_compile_option_if(-Wempty-body WEMPTY-BODY)
add_compile_option_if(-Werror-implicit-function-declaration
  WERROR-IMPLICIT-FUNCTION-DECLARATION)
add_compile_option_if(-Wextra WEXTRA)
add_compile_option_if(-Wignored-qualifiers WIGNORED-QUALIFIERS)
add_compile_option_if(-Wimplicit-fallthrough WIMPLICIT-FALLTHROUGH)
add_compile_option_if(-Wmisleading-indentation WMISLEADING-INDENTATION)
add_compile_option_if(-Wno-conversion WNO-CONVERSION)
add_compile_option_if(-Wno-sign-compare WNO-SIGN-COMPARE)
add_compile_option_if(-Wno-sign-conversion WNO-SIGN-CONVERSION)
add_compile_option_if(-Wno-unused-parameter WNO-UNUSED-PARAMETER)
add_compile_option_if(-Wstrict-prototypes WSTRICT-PROTOTYPES)
add_compile_option_if(-Wuninitialized WUNINITIALIZED)
add_compile_option_if(-Wunknown-pragmas WUNKNOWN-PRAGMAS)
add_compile_option_if(-Wunused-but-set-variable WUNUSED-BUT-SET-VARIABLE)
add_compile_option_if(-Wunused-function WUNUSED-FUNCTION)
add_compile_option_if(-Wunused-label WUNUSED-LABEL)
add_compile_option_if(-Wunused-result WUNUSED-RESULT)
add_compile_option_if(-Wunused-value WUNUSED-VALUE)
add_compile_option_if(-Wunused-variable WUNUSED-VARIABLE)
add_compile_option_if(-fno-delete-null-pointer-checks
  FNO-DELETE-NULL-POINTER-CHECKS)
add_compile_option_if(-fno-strict-overflow FNO-STRICT-OVERFLOW)

if(FLUX_USE_ASAN)
add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)
endif()
