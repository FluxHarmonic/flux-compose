#ifndef mesche_vm_h
#define mesche_vm_h

#include <stdint.h>

#include "value.h"
#include "table.h"

#define UINT8_COUNT (UINT8_MAX + 1)
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef Value (*FunctionPtr)(int arg_count, Value *args);

typedef struct {
  ObjectClosure *closure;
  uint8_t *ip;
  Value *slots;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frame_count;
  Value stack[STACK_MAX]; // TODO: Make this dynamically resizable
  Value *stack_top;
  Table strings;
  Table globals;
  ObjectUpvalue *open_upvalues;
  Object *objects;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void mesche_vm_init(VM *vm);
InterpretResult mesche_vm_run(VM *vm);
void mesche_vm_free(VM *vm);
InterpretResult mesche_vm_eval_string(VM *vm, const char *script_string);
void mesche_vm_define_native(VM *vm, const char *name, FunctionPtr function);

#endif
