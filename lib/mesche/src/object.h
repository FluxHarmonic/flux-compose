#ifndef mesche_object_h
#define mesche_object_h

#include "vm.h"
#include "value.h"

#define IS_OBJECT(value) ((value).kind == VALUE_OBJECT)
#define AS_OBJECT(value) ((value).as.object)

#define OBJECT_VAL(value) ((Value){VALUE_OBJECT, {.object = (Object*)value}})
#define OBJECT_KIND(value) (AS_OBJECT(value)->kind)

#define AS_STRING(value) ((ObjectString *)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString *)AS_OBJECT(value))->chars)

typedef enum {
  ObjectKindString
} ObjectKind;

struct Object {
  ObjectKind kind;
  struct Object *next;
};

struct ObjectString {
  struct Object object;
  uint32_t hash;
  int length;
  char chars[];
};

ObjectString *mesche_object_make_string(VM *vm, const char *chars, int length);
void mesche_object_free(struct Object *object);
void mesche_object_print(Value value);

#endif
