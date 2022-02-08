#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "value.h"
#include "object.h"

void mesche_value_array_init(ValueArray *array) {
  array->count = 0;
  array->capacity = 0;
  array->values = NULL;
}

void mesche_value_array_write(ValueArray *array, Value value) {
  if (array->capacity < array->count + 1) {
    int old_capacity = array->capacity;
    array->capacity = GROW_CAPACITY(old_capacity);
    array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
  }

  array->values[array->count] = value;
  array->count++;
}

void mesche_value_array_free(ValueArray* array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  mesche_value_array_init(array);
}

void mesche_value_print(Value value) {
  switch (value.kind) {
  case VALUE_NUMBER: printf("%g", AS_NUMBER(value)); break;
  case VALUE_NIL: printf("nil"); break;
  case VALUE_TRUE: printf("t"); break;
  case VALUE_OBJECT: mesche_object_print(value); break;
  }
}

bool mesche_value_equalp(Value a, Value b) {
  // This check also covers comparison of t and nil
  if (a.kind != b.kind) return false;

  switch(a.kind) {
  case VALUE_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
  case VALUE_OBJECT:  {
    ObjectString *a_str = AS_STRING(a);
    ObjectString *b_str = AS_STRING(b);
    return a_str->length == b_str->length && memcmp(a_str->chars, b_str->chars, a_str->length) == 0;
  }
  default: return false;
  }
}
