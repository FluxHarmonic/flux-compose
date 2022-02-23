#ifndef mesche_vm_h
#define mesche_vm_h

#include <stdint.h>

#include "mem.h"
#include "table.h"
#include "value.h"

#define UINT8_COUNT (UINT8_MAX + 1)
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef Value (*FunctionPtr)(MescheMemory *mem, int arg_count, Value *args);

typedef struct {
  ObjectClosure *closure;
  uint8_t *ip;
  Value *slots;
} CallFrame;

typedef struct {
  MescheMemory mem;
  CallFrame frames[FRAMES_MAX];
  int frame_count;
  Value stack[STACK_MAX]; // TODO: Make this dynamically resizable
  Value *stack_top;
  Table strings;
  Table symbols;

  Table modules;
  ObjectModule *root_module;
  ObjectModule *current_module;

  ObjectUpvalue *open_upvalues;
  Object *objects;

  // An opaque pointer to the current compiler to avoid cyclic type dependencies.
  // Used for calling the compiler's root marking function
  void *current_compiler;

  // Memory management
  int gray_count;
  int gray_capacity;
  Object **gray_stack;

  // An application-specific context object
  void *app_context;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void mesche_vm_init(VM *vm);
void mesche_vm_free(VM *vm);
InterpretResult mesche_vm_run(VM *vm);
InterpretResult mesche_vm_eval_string(VM *vm, const char *script_string);
void mesche_vm_stack_push(VM *vm, Value value);
Value mesche_vm_stack_pop(VM *vm);
void mesche_vm_define_native(VM *vm, const char *name, FunctionPtr function, bool exported);
void mesche_mem_mark_object(VM *vm, Object *object);

#endif
