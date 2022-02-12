#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#include "vm.h"
#include "op.h"
#include "mem.h"
#include "util.h"
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
  mesche_table_init(&vm->globals);
}

void mesche_vm_free(VM *vm) {
  mesche_table_free(&vm->strings);
  mesche_table_free(&vm->globals);
  vm_reset_stack(vm);
  vm_free_objects(vm);
}

InterpretResult mesche_vm_run(VM *vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

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
      mesche_value_print(*slot);
      printf(" ]");
    }
    printf("\n");

    mesche_disasm_instr(vm->chunk, (int)(vm->ip - vm->chunk->code));
    #endif

    uint8_t instr;
    Value value;
    ObjectString *name;
    uint8_t slot = 0;
    Value *prev_stack_top = vm->stack_top;

    switch (instr = READ_BYTE()) {
    case OP_CONSTANT:
      value = READ_CONSTANT();
      vm_stack_push(vm, value);
      break;
    case OP_NIL: vm_stack_push(vm, NIL_VAL); break;
    case OP_T: vm_stack_push(vm, T_VAL); break;
    case OP_POP: vm_stack_pop(vm); break;
    case OP_ADD: BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, +); break;
    case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, -); break;
    case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, *); break;
    case OP_DIVIDE: BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, /); break;
    case OP_AND: BINARY_OP(BOOL_VAL, IS_ANY, AS_BOOL, &&); break;
    case OP_OR: BINARY_OP(BOOL_VAL, IS_ANY, AS_BOOL, ||); break;
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
      return INTERPRET_OK;
    case OP_DISPLAY:
      // Peek at the value on the stack
      mesche_value_print(vm_stack_peek(vm, 0));
      break;
    case OP_DEFINE_GLOBAL:
      name = READ_STRING();
      mesche_table_set(&vm->globals, name, vm_stack_peek(vm, 0));
      /* vm_stack_pop(vm); // Pop value after adding entry to avoid GC */
      break;
    case OP_READ_GLOBAL:
      name = READ_STRING();
      if (!mesche_table_get(&vm->globals, name, &value)) {
        vm_runtime_error(vm, "Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      vm_stack_push(vm, value);
      break;
    case OP_READ_LOCAL:
      slot = READ_BYTE();
      vm_stack_push(vm, vm->stack[slot]);
      break;
    case OP_SET_GLOBAL:
      name = READ_STRING();
      if(mesche_table_set(&vm->globals, name, vm_stack_peek(vm, 0))) {
        vm_runtime_error(vm, "Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      /* vm_stack_pop(vm); */
      break;
    case OP_SET_LOCAL:
      slot = READ_BYTE();
      vm->stack[slot] = vm_stack_peek(vm, 0);
      break;
    }

    // For now, we enforce that all instructions except OP_POP should produce
    // (or leave) a value on the stack so that we can be consistent with how
    // expressions are popped in blocks when their values aren't consumed.
    if (instr != OP_POP && vm->stack_top < prev_stack_top) {
      PANIC("Instruction \"%d\" consumed a stack value!\n", instr);
    }
  }

#undef READ_STRING
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult mesche_vm_interpret_chunk(VM *vm, Chunk *chunk) {
  vm->chunk = chunk;
  vm->ip = vm->chunk->code;
  mesche_vm_run(vm);
}
