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

static uint32_t object_string_hash(const char *key, int length) {
  // Use the FNV-1a hash algorithm
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }

  return hash;
}

ObjectString *mesche_object_make_string(VM *vm, char *chars, int length) {
  // Is the string already interned?
  uint32_t hash = object_string_hash(chars, length);
  ObjectString *interned_string = mesche_table_find_key(&vm->strings, chars, length, hash);
  if (interned_string != NULL) return interned_string;

  // Allocate and initialize the string object
  ObjectString *string = ALLOC_OBJECT_EX(vm, ObjectString, length + 1, ObjectKindString);
  memcpy(string->chars, chars, length);
  string->chars[length + 1] = '\0';
  string->length = length;
  string->hash = hash;

  // Add the string to the interned set
  mesche_table_set(&vm->strings, string, NIL_VAL);

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