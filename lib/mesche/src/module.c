#include<linux/limits.h>
#include <stdlib.h>

#include "module.h"
#include "object.h"
#include "string.h"
#include "table.h"
#include "util.h"
#include "fs.h"

void mesche_module_print_name(ObjectModule *module) {
  printf("(%s)", module->name->chars);
}

// (mesche io) -> modules/mesche/io.msc
// (substratic graphics texture) -> deps/substratic/graphics/texture.msc

char *mesche_module_make_path(const char* base_path, const char *module_name) {
  int start_index = strlen(base_path);
  char *module_path = malloc(sizeof(char) * 1000);

  memcpy(module_path, base_path, start_index);
  module_path[start_index++] = '/';

  for (int i = 0; i < strlen(module_name); i++) {
    module_path[start_index++] = module_name[i] == ' ' ? '/' : module_name[i];
  }

  // Add the file extension to the last part of the path
  memcpy(module_path + start_index, ".msc", 4);
  module_path[start_index + 4] = 0;

  return module_path;
}

char *mesche_module_find_module_path(VM *vm, const char *module_name) {
  // TODO: Check for path variations --
  // mesche/io.msc
  // mesche/io/module.msc

  ObjectCons *load_path_entry = vm->load_paths;
  while (load_path_entry) {
    char *module_path = mesche_module_make_path(AS_CSTRING(load_path_entry->car), module_name);
    if (mesche_fs_path_exists_p(module_path)) {
      return module_path;
    }

    if (module_path) {
      free(module_path);
    }

    // Search the next load path
    load_path_entry = IS_CONS(load_path_entry->cdr) ? AS_CONS(load_path_entry->cdr) : NULL;
  }

  return NULL;
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
  // First, check if the module is already loaded, return it if so
  // Then check the load path to see if a module file exists
  // If no file exists, create a new module with the name

  Value module_val;
  ObjectModule *module = NULL;
  if (!mesche_table_get(&vm->modules, module_name, &module_val)) {
    // Create an empty module
    module = mesche_object_make_module(vm, module_name);
    mesche_table_set((MescheMemory *)vm, &vm->modules, module_name, OBJECT_VAL(module));

    // Look up the module in the load path
    char *module_path = mesche_module_find_module_path(vm, module_name->chars);
    if (module_path) {
      // Load the module file to populate the empty module
      // TODO: Guard against circular module loads!
      mesche_vm_load_module(vm, module_path);
      free(module_path);

      if (!mesche_table_get(&vm->modules, module_name, &module_val)) {
        PANIC("Could not resolve module (%s) after loading its file.", module_name);
      }

       module = AS_MODULE(module_val);
    }
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
