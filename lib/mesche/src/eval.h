#ifndef mesche_eval_h
#define mesche_eval_h

#include "vm.h"

extern InterpretResult mesche_eval_string(VM *vm, const char *script_source);

#endif
