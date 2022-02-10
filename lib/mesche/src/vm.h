#ifndef mesche_vm_h
#define mesche_vm_h

#include <stdint.h>

#include "value.h"
#include "chunk.h"
#include "table.h"

#define VM_STACK_MAX 256

typedef struct {
  Chunk *chunk;
  uint8_t *ip;
  Value stack[VM_STACK_MAX]; // TODO: Make this dynamically resizable
  Value *stack_top;
  Table strings;
  Object *objects;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern void mesche_vm_init(VM *vm);
InterpretResult mesche_vm_run(VM *vm);
extern void mesche_vm_free(VM *vm);

#endif
