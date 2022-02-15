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

#define IS_CLOSURE(value) object_is_kind(value, ObjectKindClosure)
#define AS_CLOSURE(value) ((ObjectClosure *)AS_OBJECT(value))

#define IS_NATIVE_FUNC(value) object_is_kind(value, ObjectKindNativeFunction)
#define AS_NATIVE_FUNC(value) (((ObjectNativeFunction *)AS_OBJECT(value))->function)

#define AS_STRING(value) ((ObjectString *)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString *)AS_OBJECT(value))->chars)

typedef enum {
  ObjectKindString,
  ObjectKindSymbol,
  ObjectKindKeyword,
  ObjectKindUpvalue,
  ObjectKindFunction,
  ObjectKindClosure,
  ObjectKindNativeFunction
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

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT
} FunctionType;

struct ObjectFunction {
  Object object;
  FunctionType type;
  int arity;
  int upvalue_count;
  Chunk chunk;
  ObjectString *name;
};

struct ObjectUpvalue {
  Object object;
  Value *location;
  Value closed;
  struct ObjectUpvalue *next;
};

struct ObjectClosure {
  Object object;
  ObjectFunction *function;
  ObjectUpvalue **upvalues;
  int upvalue_count;
};

typedef struct {
  Object object;
  FunctionPtr function;
} ObjectNativeFunction;

ObjectString *mesche_object_make_string(VM *vm, const char *chars, int length);
ObjectUpvalue *mesche_object_make_upvalue(VM *vm, Value *slot);
ObjectFunction *mesche_object_make_function(VM *vm, FunctionType type);
ObjectClosure *mesche_object_make_closure(VM *vm, ObjectFunction *function);
ObjectNativeFunction *mesche_object_make_native_function(VM *vm, FunctionPtr function);

void mesche_object_free(struct Object *object);
void mesche_object_print(Value value);

#endif
