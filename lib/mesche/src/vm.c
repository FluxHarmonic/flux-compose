#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#include "vm.h"
#include "op.h"
#include "mem.h"
#include "chunk.h"
#include "value.h"
#include "disasm.h"
#include "object.h"

// NOTE: Enable this for diagnostic purposes
/* #define DEBUG_TRACE_EXECUTION */

static void vm_stack_push(VM *vm, Value value) {
  *vm->stack_top = value;
  vm->stack_top++;
}

static Value vm_stack_pop(VM *vm) {
  vm->stack_top--;
  return *vm->stack_top;
}

static Value vm_stack_peek(VM *vm, int distance) {
  return vm->stack_top[-1 - distance];
}

static void vm_reset_stack(VM *vm) {
  vm->stack_top = vm->stack;
  vm->objects = NULL;
}

static void vm_runtime_error(VM *vm, const char *format, ...) {
  // TODO: Port to printf
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm->ip - vm->chunk->code - 1;
  int line = vm->chunk->lines[instruction];
  fprintf(stderr, "[line %d] in script\n", line);

  vm_reset_stack(vm);
}
static void vm_free_objects(VM *vm) {
  Object *object = vm->objects;
  while (object != NULL) {
    Object *next = object->next;
    mesche_object_free(object);
    object = next;
  }
}

void mesche_vm_init(VM *vm) {
  vm->ip = NULL;
  vm->chunk = NULL;
  vm->objects = NULL;
  vm_reset_stack(vm);
  mesche_table_init(&vm->strings);
}

void mesche_vm_free(VM *vm) {
  mesche_table_free(&vm->strings);
  vm_reset_stack(vm);
  vm_free_objects(vm);
}

InterpretResult mesche_vm_run(VM *vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])

// TODO: Don't pop 'a', manipulate top of stack
#define BINARY_OP(value_type, pred, cast, op)   \
  do { \
    if (!pred(vm_stack_peek(vm, 0)) || !pred(vm_stack_peek(vm, 1))) { \
      vm_runtime_error(vm, "Operands must be numbers.");                 \
      return INTERPRET_RUNTIME_ERROR; \
    } \
    double b = cast(vm_stack_pop(vm));      \
    double a = cast(vm_stack_pop(vm));      \
    vm_stack_push(vm, value_type(a op b));       \
  } while (false)

  for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
    printf(" ");
    for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
      printf("[ ");
      flux_script_print_value(*slot);
      printf(" ]");
    }
    printf("\n");

    mesche_disasm_instr(vm->chunk, (int)(vm->ip - vm->chunk->code));
    #endif

    uint8_t instr;
    Value constant;
    switch (instr = READ_BYTE()) {
    case OP_CONSTANT:
      constant = READ_CONSTANT();
      vm_stack_push(vm, constant);
      break;
    case OP_NIL: vm_stack_push(vm, NIL_VAL); break;
    case OP_T: vm_stack_push(vm, T_VAL); break;
    case OP_ADD: BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, +); break;
    case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, -); break;
    case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, *); break;
    case OP_DIVIDE: BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, /); break;
    case OP_AND: BINARY_OP(BOOL_VAL, IS_ANY, AS_BOOL, &&); break;
    case OP_OR: BINARY_OP(BOOL_VAL, IS_ANY, AS_BOOL, ||); break;
      // TODO: The OR operator doesn't work exactly right!
    case OP_NOT: vm_stack_push(vm, IS_NIL(vm_stack_pop(vm)) ? T_VAL : NIL_VAL); break;
    case OP_EQUAL:
      // Drop through for now
    case OP_EQV: {
      Value b = vm_stack_pop(vm);
      Value a = vm_stack_pop(vm);
      vm_stack_push(vm, BOOL_VAL(mesche_value_equalp(a, b)));
      break;
    }
    case OP_RETURN:
      mesche_value_print(vm_stack_pop(vm));
      printf("\n");
      return INTERPRET_OK;
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult mesche_vm_interpret_chunk(VM *vm, Chunk *chunk) {
  vm->chunk = chunk;
  vm->ip = vm->chunk->code;
  mesche_vm_run(vm);
}
