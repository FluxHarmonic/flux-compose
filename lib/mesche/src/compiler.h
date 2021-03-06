#ifndef mesche_compiler_h
#define mesche_compiler_h

#include "vm.h"
#include "scanner.h"

ObjectFunction *mesche_compile_source(VM *vm, const char *script_source);
void mesche_compiler_mark_roots(void *target);

#endif
