#ifndef mesche_list_h
#define mesche_list_h

#include "vm.h"

ObjectCons *mesche_list_push(VM *vm, ObjectCons *list, Value value);

#endif
