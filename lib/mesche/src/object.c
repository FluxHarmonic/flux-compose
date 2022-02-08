#include <stdio.h>

#include "vm.h"
#include "mem.h"
#include "util.h"
#include "object.h"

#define ALLOC_OBJECT(vm, type, object_kind)            \
  (type *)object_allocate(vm, sizeof(type), object_kind)

#define ALLOC_OBJECT_EX(vm, type, extra_size, object_kind)  \
  (type *)object_allocate(vm, sizeof(type) + extra_size, object_kind)

static inline bool object_is_kind(Value value, ObjectKind kind) {
  return IS_OBJECT(value) && AS_OBJECT(value)->kind == kind;
}

static Object* object_allocate(VM *vm, size_t size, ObjectKind kind) {
  Object *object = (Object*)mesche_mem_realloc(NULL, 0, size);
  object->kind = kind;

  // Keep track of the object for garbage collection
  object->next = vm->objects;
  vm->objects = object;

  return object;
}

ObjectString *mesche_object_make_string(VM *vm, char *chars, int length) {
  ObjectString *string = ALLOC_OBJECT_EX(vm, ObjectString, length + 1, ObjectKindString);
  memcpy(string->chars, chars, length);
  string->chars[length + 1] = '\0';
  string->length = length;

  return string;
}


void mesche_object_free(Object *object) {
  switch (object->kind) {
  case ObjectKindString:
    ObjectString *string = (ObjectString*)object;
    FREE_SIZE(string, (sizeof(ObjectString) + string->length + 1));
    break;
  default:
    PANIC("Don't know how to free object kind %d!", object->kind);
  }
}

void mesche_object_print(Value value) {
  switch(OBJECT_KIND(value)) {
  case ObjectKindString:
    printf("\"%s\"", AS_CSTRING(value));
    break;
  }
}
