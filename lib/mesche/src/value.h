#ifndef mesche_value_h
#define mesche_value_h

typedef struct Object Object;
typedef struct ObjectString ObjectString;

#include <stdbool.h>

#define T_VAL ((Value){VALUE_TRUE, {.number = 0}})
#define NIL_VAL ((Value){VALUE_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VALUE_NUMBER, {.number = value}})
#define BOOL_VAL(value) ((Value) { value ? VALUE_TRUE : VALUE_NIL, {.number = 0}})

#define AS_NUMBER(value) ((value).as.number)
#define AS_BOOL(value) ((value).kind != VALUE_NIL)

#define IS_ANY(value) (true)
#define IS_T(value) ((value).kind == ValueKindTrue)
#define IS_NIL(value) ((value).kind == VALUE_NIL)
#define IS_NUMBER(value) ((value).kind == VALUE_NUMBER)
#define IS_STRING(value) object_is_kind(value, ObjectKindString)

typedef enum {
  VALUE_NIL,
  VALUE_TRUE,
  VALUE_NUMBER,
  VALUE_STRING,
  VALUE_OBJECT
} ValueKind;

typedef struct {
  ValueKind kind;
  union {
    bool boolean;
    double number;
    Object *object;
  } as;
} Value;

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

void mesche_value_array_init(ValueArray *array);
void mesche_value_array_write(ValueArray *array, Value value);
void mesche_value_array_free(ValueArray* array);
void mesche_value_print(Value value);
bool mesche_value_equalp(Value a, Value b);

#endif
