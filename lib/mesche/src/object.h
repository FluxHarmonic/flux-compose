#ifndef mesche_object_h
#define mesche_object_h

#include "vm.h"
#include "chunk.h"
#include "value.h"

#define IS_OBJECT(value) ((value).kind == VALUE_OBJECT)
#define AS_OBJECT(value) ((value).as.object)

#define OBJECT_VAL(value) ((Value){VALUE_OBJECT, {.object = (Object*)value}})
#define OBJECT_KIND(value) (AS_OBJECT(value)->kind)

#define IS_FUNCTION(value) object_is_kind(value, ObjectKindFunction)
#define AS_FUNCTION(value) ((ObjectFunction *)AS_OBJECT(value))

#define AS_STRING(value) ((ObjectString *)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString *)AS_OBJECT(value))->chars)

typedef enum {
  ObjectKindString,
  ObjectKindSymbol,
  ObjectKindKeyword,
  ObjectKindFunction
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

struct ObjectSymbol {
  struct Object object;
  uint32_t hash;
  int length;
  char chars[];
};

struct ObjectFunction {
  Object object;
  int arity;
  Chunk chunk;
  ObjectString *name;
};

ObjectString *mesche_object_make_string(VM *vm, const char *chars, int length);
ObjectFunction *mesche_object_make_function(VM *vm);

void mesche_object_free(struct Object *object);
void mesche_object_print(Value value);

#endif
