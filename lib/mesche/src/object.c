#include <stdio.h>

#include "mem.h"
#include "object.h"
#include "util.h"
#include "vm.h"

#define ALLOC(vm, type, count)                                                                     \
  (type) mesche_mem_realloc((MescheMemory *)vm, NULL, 0, sizeof(type) * count)

#define ALLOC_OBJECT(vm, type, object_kind) (type *)object_allocate(vm, sizeof(type), object_kind)

#define ALLOC_OBJECT_EX(vm, type, extra_size, object_kind)                                         \
  (type *)object_allocate(vm, sizeof(type) + extra_size, object_kind)

static Object *object_allocate(VM *vm, size_t size, ObjectKind kind) {
  Object *object = (Object *)mesche_mem_realloc((MescheMemory *)vm, NULL, 0, size);
  object->kind = kind;

  // Keep track of the object for garbage collection
  object->is_marked = false;
  object->next = vm->objects;
  vm->objects = object;

#ifdef DEBUG_LOG_GC
  printf("%p    allocate %zu for %d\n", (void *)object, size, kind);
#endif

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

ObjectString *mesche_object_make_string(VM *vm, const char *chars, int length) {
  // Is the string already interned?
  uint32_t hash = object_string_hash(chars, length);
  ObjectString *interned_string = mesche_table_find_key(&vm->strings, chars, length, hash);
  if (interned_string != NULL)
    return interned_string;

  // Allocate and initialize the string object
  ObjectString *string = ALLOC_OBJECT_EX(vm, ObjectString, length + 1, ObjectKindString);
  memcpy(string->chars, chars, length);
  string->chars[length] = '\0';
  string->length = length;
  string->hash = hash;

  // Push the string onto the stack temporarily to avoid GC
  mesche_vm_stack_push(vm, OBJECT_VAL(string));

  // Add the string to the interned set
  mesche_table_set((MescheMemory *)vm, &vm->strings, string, NIL_VAL);

  // Pop the string back off the stack
  mesche_vm_stack_pop(vm);

  return string;
}

ObjectKeyword *mesche_object_make_keyword(VM *vm, const char *chars, int length) {
  // TODO: Do we want to intern keyword strings too?
  // Allocate and initialize the string object
  uint32_t hash = object_string_hash(chars, length);
  ObjectKeyword *keyword = ALLOC_OBJECT_EX(vm, ObjectKeyword, length + 1, ObjectKindKeyword);
  memcpy(keyword->string.chars, chars, length);
  keyword->string.chars[length] = '\0';
  keyword->string.length = length;
  keyword->string.hash = hash;

  return (ObjectKeyword *)keyword;
}

ObjectString *mesche_object_make_symbol(VM *vm, const char *chars, int length) {
  // Is the string already interned?
  uint32_t hash = object_string_hash(chars, length);
  ObjectString *interned_string = mesche_table_find_key(&vm->strings, chars, length, hash);
  if (interned_string != NULL)
    return interned_string;

  // Allocate and initialize the string object
  ObjectString *string = ALLOC_OBJECT_EX(vm, ObjectString, length + 1, ObjectKindString);
  memcpy(string->chars, chars, length);
  string->chars[length] = '\0';
  string->length = length;
  string->hash = hash;

  // Add the string to the interned set
  mesche_table_set((MescheMemory *)vm, &vm->strings, string, NIL_VAL);

  return string;
}

ObjectUpvalue *mesche_object_make_upvalue(VM *vm, Value *slot) {
  ObjectUpvalue *upvalue = ALLOC_OBJECT(vm, ObjectUpvalue, ObjectKindUpvalue);
  upvalue->location = slot;
  upvalue->next = NULL;
  upvalue->closed = NIL_VAL;
  return upvalue;
}

static void function_keyword_args_init(KeywordArgumentArray *array) {
  array->count = 0;
  array->capacity = 0;
  array->args = NULL;
}

static void function_keyword_args_free(MescheMemory *mem, KeywordArgumentArray *array) {
  FREE_ARRAY(mem, Value, array->args, array->capacity);
  function_keyword_args_init(array);
}

void mesche_object_function_keyword_add(MescheMemory *mem, ObjectFunction *function,
                                        KeywordArgument keyword_arg) {
  KeywordArgumentArray *array = &function->keyword_args;
  if (array->capacity < array->count + 1) {
    int old_capacity = array->capacity;
    array->capacity = GROW_CAPACITY(old_capacity);
    array->args = GROW_ARRAY(mem, KeywordArgument, array->args, old_capacity, array->capacity);
  }

  array->args[array->count] = keyword_arg;
  array->count++;
}

ObjectFunction *mesche_object_make_function(VM *vm, FunctionType type) {
  ObjectFunction *function = ALLOC_OBJECT(vm, ObjectFunction, ObjectKindFunction);
  function->arity = 0;
  function->upvalue_count = 0;
  function->type = type;
  function->name = NULL;
  function_keyword_args_init(&function->keyword_args);
  mesche_chunk_init(&function->chunk);

  return function;
}

ObjectClosure *mesche_object_make_closure(VM *vm, ObjectFunction *function) {
  // Allocate upvalues for this closure instance
  ObjectUpvalue **upvalues = ALLOC(vm, ObjectUpvalue **, function->upvalue_count);
  for (int i = 0; i < function->upvalue_count; i++) {
    upvalues[i] = NULL;
  }

  ObjectClosure *closure = ALLOC_OBJECT(vm, ObjectClosure, ObjectKindClosure);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalue_count = function->upvalue_count;
  return closure;
}

ObjectNativeFunction *mesche_object_make_native_function(VM *vm, FunctionPtr function) {
  ObjectNativeFunction *native = ALLOC_OBJECT(vm, ObjectNativeFunction, ObjectKindNativeFunction);
  native->function = function;
  return native;
}

ObjectPointer *mesche_object_make_pointer(VM *vm, void *ptr, bool is_managed) {
  ObjectPointer *pointer = ALLOC_OBJECT(vm, ObjectPointer, ObjectKindPointer);
  pointer->ptr = ptr;
  pointer->is_managed = is_managed;
  return pointer;
}

void mesche_object_free(VM *vm, Object *object) {
#ifdef DEBUG_LOG_GC
  printf("%p    free   ", (void *)object);
  mesche_value_print(OBJECT_VAL(object));
  printf("\n");
#endif

  switch (object->kind) {
  case ObjectKindString: {
    ObjectString *string = (ObjectString *)object;
    FREE_SIZE(vm, string, (sizeof(ObjectString) + string->length + 1));
    break;
  }
  case ObjectKindKeyword: {
    ObjectKeyword *keyword = (ObjectKeyword *)object;
    FREE_SIZE(vm, keyword, (sizeof(ObjectKeyword) + keyword->string.length + 1));
    break;
  }
  case ObjectKindUpvalue:
    FREE(vm, ObjectUpvalue, object);
    break;
  case ObjectKindFunction:
    FREE(vm, ObjectFunction, object);
    break;
  case ObjectKindClosure: {
    ObjectClosure *closure = (ObjectClosure *)object;
    FREE_ARRAY(vm, ObjectUpvalue *, closure->upvalues, closure->upvalue_count);
    FREE(vm, ObjectFunction, object);
    break;
  }
  case ObjectKindNativeFunction:
    FREE(vm, ObjectNativeFunction, object);
    break;
  case ObjectKindPointer: {
    ObjectPointer *pointer = (ObjectPointer *)object;
    if (pointer->is_managed) {
      printf("FREEING NATIVE POINTER!\n");
      free(pointer->ptr);
      FREE(vm, ObjectPointer, object);
      break;
    }
  }
    FREE(vm, ObjectNativeFunction, object);
    break;
  default:
    PANIC("Don't know how to free object kind %d!", object->kind);
  }
}

static void print_function(ObjectFunction *function) {
  if (function->name == NULL) {
    if (function->type == TYPE_SCRIPT) {
      printf("<script 0x%x>", function);
    } else {
      printf("<lambda 0x%x>", function);
    }
    return;
  }

  printf("<fn %s 0x%x>", function->name->chars, function);
}

void mesche_object_print(Value value) {
  switch (OBJECT_KIND(value)) {
  case ObjectKindString:
    printf("%s", AS_CSTRING(value));
    break;
  case ObjectKindKeyword:
    printf(":%s", AS_CSTRING(value));
    break;
  case ObjectKindUpvalue:
    printf("upvalue");
    break;
  case ObjectKindFunction:
    print_function(AS_FUNCTION(value));
    break;
  case ObjectKindClosure:
    print_function(AS_CLOSURE(value)->function);
    break;
  case ObjectKindNativeFunction:
    printf("<native fn>");
    break;
  case ObjectKindPointer:
    printf("<pointer %p>", AS_POINTER(value)->ptr);
    break;
  }
}

inline bool mesche_object_is_kind(Value value, ObjectKind kind) {
  return IS_OBJECT(value) && AS_OBJECT(value)->kind == kind;
}

bool mesche_object_string_equalsp(Object *left, Object *right) {
  if (!(left->kind == ObjectKindString || left->kind == ObjectKindKeyword) &&
      !(right->kind == ObjectKindString || right->kind == ObjectKindKeyword)) {
    return false;
  }

  ObjectString *left_str = (ObjectString *)left;
  ObjectString *right_str = (ObjectString *)right;

  return (left_str->length == right_str->length && left_str->hash == right_str->hash &&
          memcmp(left_str->chars, right_str->chars, left_str->length) == 0);
}
