#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#include "vm.h"
#include "compiler.h"
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
  vm->frame_count = 0;
  vm->objects = NULL;
}

static void vm_runtime_error(VM *vm, const char *format, ...) {
  CallFrame *frame = &vm->frames[vm->frame_count - 1];

  // TODO: Port to printf
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = frame->ip - frame->function->chunk.code - 1;
  int line = frame->function->chunk.lines[instruction];
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

static bool vm_call(VM *vm, ObjectFunction *function, uint8_t arg_count) {
  CallFrame *frame = &vm->frames[vm->frame_count++];
  frame->function = function;
  frame->ip = function->chunk.code;
  frame->slots = vm->stack_top - arg_count - 1;
  return true;
}

static bool vm_call_value(VM *vm, Value callee, uint8_t arg_count) {
  if (IS_OBJECT(callee)) {
    switch (OBJECT_KIND(callee)) {
    case ObjectKindFunction:
      return vm_call(vm, AS_FUNCTION(callee), arg_count);
    case ObjectKindNativeFunction:
      FunctionPtr func_ptr = AS_NATIVE_FUNC(callee);
      Value result = func_ptr(arg_count, vm->stack_top - arg_count);
      vm_stack_push(vm, result);
      return true;
    default: break; // Value not callable
    }
  }

  vm_runtime_error(vm, "Can functions can be called.");
  return false;
}

InterpretResult mesche_vm_run(VM *vm) {
  CallFrame *frame = &vm->frames[vm->frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
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

    mesche_disasm_instr(frame->function->chunk, (int)(frame->ip - frame->function->chunk.code));
    #endif

    uint8_t instr;
    uint8_t offset;
    Value value;
    ObjectString *name;
    uint8_t slot = 0;
    uint8_t arg_count;
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
    case OP_JUMP:
      offset = READ_SHORT();
      frame->ip += offset;
      break;
    case OP_JUMP_IF_FALSE:
      offset = READ_SHORT();
      if (IS_FALSEY(vm_stack_peek(vm, 0))) {
        frame->ip += offset;
      }
      break;
    case OP_RETURN:
      // Hold on to the function result value before we manipulate the stack
      value = vm_stack_pop(vm);

      // If we're out of call frames, end execution
      vm->frame_count--;
      if (vm->frame_count == 0) {
        vm_stack_pop(vm);
        return INTERPRET_OK;
      }

      // Restore the previous result value, call frame, and value stack pointer
      // before continuing execution
      vm->stack_top = frame->slots;
      vm_stack_push(vm, value);
      frame = &vm->frames[vm->frame_count - 1];
      break;
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
      vm_stack_push(vm, frame->slots[slot]);
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
      frame->slots[slot] = vm_stack_peek(vm, 0);
      break;
    case OP_CALL:
      // Call the function with the specified number of arguments
      arg_count = READ_BYTE();
      if (!vm_call_value(vm, vm_stack_peek(vm, arg_count), arg_count)) {
        return INTERPRET_COMPILE_ERROR;
      }

      // Return to the previous call frame
      frame = &vm->frames[vm->frame_count - 1];
      break;
    }

    // For now, we enforce that all instructions except OP_POP should produce
    // (or leave) a value on the stack so that we can be consistent with how
    // expressions are popped in blocks when their values aren't consumed.
    // TODO: Rethink this check, it doesn't work for binary operations which
    // pop 2 values from the stack
    /* if (instr != OP_POP && vm->stack_top < prev_stack_top) { */
    /*   PANIC("Instruction \"%d\" consumed a stack value! (before: %x, after: %x)\n", instr, prev_stack_top, vm->stack_top); */
    /* } */
  }

#undef READ_STRING
#undef READ_SHORT
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

static Value mesche_vm_clock_native(int arg_count, Value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

void mesche_vm_define_native(VM *vm, const char *name, FunctionPtr function) {
  // Create objects for the name and the function
  vm_stack_push(vm, OBJECT_VAL(mesche_object_make_string(vm, name, (int)strlen(name))));
  vm_stack_push(vm, OBJECT_VAL(mesche_object_make_native_function(vm, function)));

  // Add the item to the table and pop them back out
  mesche_table_set(&vm->globals, AS_STRING(*(vm->stack_top - 2)), *(vm->stack_top - 1));
  vm_stack_pop(vm);
  vm_stack_pop(vm);
}

InterpretResult mesche_vm_eval_string(VM *vm, const char *script_string) {
  ObjectFunction *function = mesche_compile_source(vm, script_string);
  if (function == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }

  // Push the top-level function and call it
  vm_stack_push(vm, OBJECT_VAL(function));
  vm_call(vm, function, 0);

  // Define core native functions
  mesche_vm_define_native(vm, "clock", mesche_vm_clock_native);

  // Run the VM starting at the first call frame
  return mesche_vm_run(vm);
}
