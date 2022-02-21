#ifndef mesche_object_h
#define mesche_object_h

#include "chunk.h"
#include "value.h"
#include "vm.h"

#define IS_OBJECT(value) ((value).kind == VALUE_OBJECT)
#define AS_OBJECT(value) ((value).as.object)

#define OBJECT_VAL(value) ((Value){VALUE_OBJECT, {.object = (Object *)value}})
#define OBJECT_KIND(value) (AS_OBJECT(value)->kind)

#define IS_CONS(value) mesche_object_is_kind(value, ObjectKindCons)
#define AS_CONS(value) ((ObjectCons *)AS_OBJECT(value))

#define IS_FUNCTION(value) mesche_object_is_kind(value, ObjectKindFunction)
#define AS_FUNCTION(value) ((ObjectFunction *)AS_OBJECT(value))

#define IS_KEYWORD(value) mesche_object_is_kind(value, ObjectKindKeyword)
#define AS_KEYWORD(value) ((ObjectKeyword *)AS_OBJECT(value))

#define IS_POINTER(value) mesche_object_is_kind(value, ObjectKindPointer)
#define AS_POINTER(value) ((ObjectPointer *)AS_OBJECT(value))

#define IS_CLOSURE(value) mesche_object_is_kind(value, ObjectKindClosure)
#define AS_CLOSURE(value) ((ObjectClosure *)AS_OBJECT(value))

#define IS_NATIVE_FUNC(value) mesche_object_is_kind(value, ObjectKindNativeFunction)
#define AS_NATIVE_FUNC(value) (((ObjectNativeFunction *)AS_OBJECT(value))->function)

#define AS_STRING(value) ((ObjectString *)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString *)AS_OBJECT(value))->chars)

typedef enum {
  ObjectKindString,
  ObjectKindSymbol,
  ObjectKindKeyword,
  ObjectKindCons,
  ObjectKindUpvalue,
  ObjectKindFunction,
  ObjectKindClosure,
  ObjectKindNativeFunction,
  ObjectKindPointer
} ObjectKind;

struct Object {
  ObjectKind kind;
  bool is_marked;
  struct Object *next;
};

struct ObjectString {
  struct Object object;
  uint32_t hash;
  int length;
  char chars[];
};

struct ObjectKeyword {
  // A keyword is basically a tagged string
  struct ObjectString string;
};

struct ObjectSymbol {
  struct Object object;
  uint32_t hash;
  int length;
  char chars[];
};

typedef enum { TYPE_FUNCTION, TYPE_SCRIPT } FunctionType;

typedef struct {
  ObjectString *name;
  uint8_t default_index;
} KeywordArgument;

typedef struct {
  int capacity;
  int count;
  KeywordArgument *args;
} KeywordArgumentArray;

struct ObjectFunction {
  Object object;
  FunctionType type;
  int arity;
  int upvalue_count;
  Chunk chunk;
  KeywordArgumentArray keyword_args;
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

typedef struct {
  Object object;
  void *ptr;
  bool is_managed;
} ObjectPointer;

ObjectString *mesche_object_make_string(VM *vm, const char *chars, int length);
ObjectKeyword *mesche_object_make_keyword(VM *vm, const char *chars, int length);
ObjectUpvalue *mesche_object_make_upvalue(VM *vm, Value *slot);
ObjectFunction *mesche_object_make_function(VM *vm, FunctionType type);
void mesche_object_function_keyword_add(MescheMemory *mem, ObjectFunction *function,
                                        KeywordArgument keyword_arg);
ObjectClosure *mesche_object_make_closure(VM *vm, ObjectFunction *function);
ObjectNativeFunction *mesche_object_make_native_function(VM *vm, FunctionPtr function);
ObjectPointer *mesche_object_make_pointer(VM *vm, void *ptr, bool is_managed);

void mesche_object_free(VM *vm, struct Object *object);
void mesche_object_print(Value value);

bool mesche_object_is_kind(Value value, ObjectKind kind);
bool mesche_object_string_equalsp(Object *left, Object *right);

#endif
