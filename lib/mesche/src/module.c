#include "module.h"
#include "object.h"
#include "string.h"
#include "table.h"

void mesche_module_print_name(ObjectModule *module) {
  printf("(%s)", module->name->chars);
}

ObjectString *mesche_module_name_from_symbol_list(VM *vm, ObjectCons *list) {
  ObjectString *module_name = AS_STRING(list->car);
  ObjectString *current_name = module_name;
  while (IS_CONS(list->cdr)) {
    list = AS_CONS(list->cdr);
    current_name = AS_STRING(list->car);
    module_name = mesche_string_join(vm, module_name, current_name, " ");
  }

  return module_name;
}

static ObjectModule *mesche_module_resolve_by_name(VM *vm, ObjectString *module_name) {
  Value module_val;
  ObjectModule *module = NULL;
  if (!mesche_table_get(&vm->modules, module_name, &module_val)) {
    // Register the module
    // TODO: Attempt to load the module from the load path!
    module = mesche_object_make_module(vm, module_name);
    mesche_table_set((MescheMemory *)vm, &vm->modules, module_name, OBJECT_VAL(module));
  } else {
    module = AS_MODULE(module_val);
  }

  return module;
}

static ObjectModule *mesche_module_resolve_by_name_string(VM *vm, const char *module_name) {
  ObjectString *module_name_str = mesche_object_make_string(vm, module_name, strlen(module_name));
  return mesche_module_resolve_by_name(vm, module_name_str);
}

static ObjectModule *mesche_module_resolve_by_path(VM *vm, ObjectCons *list) {
  ObjectString *module_name = mesche_module_name_from_symbol_list(vm, list);
  return mesche_module_resolve_by_name(vm, module_name);
}

void mesche_module_enter_path(VM *vm, ObjectCons *list) {
  vm->current_module = mesche_module_resolve_by_path(vm, list);
}

void mesche_module_enter(VM *vm, ObjectModule *module) {
  vm->current_module = module;
}

void mesche_module_enter_by_name(VM *vm, const char *module_name) {
  mesche_module_enter(vm, mesche_module_resolve_by_name_string(vm, module_name));
}

void mesche_module_import_path(VM *vm, ObjectCons *list) {
  // TODO: Warn or error on shadowing?
  ObjectModule *current_module = vm->current_module;
  ObjectModule *module = mesche_module_resolve_by_path(vm, list);
  // Look up the value for each exported symbol and bind it in the current module
  for (int i = 0; i < module->exports.count; i++) {
    Value export_value;
    ObjectString *export_name = AS_STRING(module->exports.values[i]);
    mesche_table_get(&module->locals, export_name, &export_value);
    mesche_table_set((MescheMemory *)vm, &current_module->locals, export_name, export_value);
  }
}
