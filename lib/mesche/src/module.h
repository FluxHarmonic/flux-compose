#ifndef mesche_module_h
#define mesche_module_h

#include "object.h"
#include "vm.h"

void mesche_module_print_name(ObjectModule *module);
void mesche_module_enter(VM *vm, ObjectModule *module);
void mesche_module_enter_path(VM *vm, ObjectCons *list);
void mesche_module_enter_by_name(VM *vm, const char *module_name);
void mesche_module_import_path(VM *vm, ObjectCons *list);

#endif
