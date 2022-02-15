#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "chunk.h"
#include "compiler.h"
#include "disasm.h"
#include "mem.h"
#include "object.h"
#include "op.h"
#include "util.h"
#include "value.h"
#include "vm.h"

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
  vm->open_upvalues = NULL;
}

static void vm_runtime_error(VM *vm, const char *format, ...) {
  CallFrame *frame = &vm->frames[vm->frame_count - 1];

  // TODO: Port to printf
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = frame->ip - frame->closure->function->chunk.code - 1;
  int line = frame->closure->function->chunk.lines[instruction];
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

static bool vm_call(VM *vm, ObjectClosure *closure, uint8_t arg_count) {
  // TODO: Update this to support varargs
  if (arg_count != closure->function->arity) {
    vm_runtime_error(vm, "Expected %d arguments but got %d.", closure->function->arity, arg_count);
    return false;
  }

  CallFrame *frame = &vm->frames[vm->frame_count++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm->stack_top - arg_count - 1;
  return true;
}

static bool vm_call_value(VM *vm, Value callee, uint8_t arg_count) {
  if (IS_OBJECT(callee)) {
    switch (OBJECT_KIND(callee)) {
    case ObjectKindClosure:
      return vm_call(vm, AS_CLOSURE(callee), arg_count);
    case ObjectKindNativeFunction: {
      FunctionPtr func_ptr = AS_NATIVE_FUNC(callee);
      Value result = func_ptr(arg_count, vm->stack_top - arg_count);
      vm_stack_push(vm, result);
      return true;
    }
    default:
      break; // Value not callable
    }
  }

  vm_runtime_error(vm, "Only functions can be called.");
  return false;
}

static ObjectUpvalue *vm_capture_upvalue(VM *vm, Value *local) {
  ObjectUpvalue *prev_upvalue = NULL;
  ObjectUpvalue *upvalue = vm->open_upvalues;

  // Loop overall upvalues until we reach a local that is defined on the stack
  // before the local we're trying to capture.  This linked list is in reverse
  // order (top of stack comes first)
  while (upvalue != NULL && upvalue->location > local) {
    prev_upvalue = upvalue;
    upvalue = upvalue->next;
  }

  // If we found an existing upvalue for this local, return it
  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  // We didn't find an existing upvalue, create a new one and insert it
  // in the VM's upvalues linked list at the place where the loop stopped
  // (or at the beginning if NULL)
  ObjectUpvalue *created_upvalue = mesche_object_make_upvalue(vm, local);
  created_upvalue->next = upvalue;
  if (prev_upvalue == NULL) {
    // This upvalue is now the first entry
    vm->open_upvalues = created_upvalue;
  } else {
    // Because the captured local can be earlier in the stack than the
    // existing upvalue, we insert it between the previous and current
    // upvalue entries
    prev_upvalue->next = created_upvalue;
  }

  return created_upvalue;
}

static void vm_close_upvalues(VM *vm, Value *stack_slot) {
  // Loop over the upvalues and close any that are at or above the given slot
  // location on the stack
  while (vm->open_upvalues != NULL && vm->open_upvalues->location >= stack_slot) {
    // Copy the value of the local at the given location into the `closed` field
    // and then set `location` to it so that existing code uses the same pointer
    // indirection to access it regardless of whether open or closed
    ObjectUpvalue *upvalue = vm->open_upvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm->open_upvalues = upvalue->next;
  }
}

InterpretResult mesche_vm_run(VM *vm) {
  CallFrame *frame = &vm->frames[vm->frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

// TODO: Don't pop 'a', manipulate top of stack
#define BINARY_OP(value_type, pred, cast, op)                                                      \
  do {                                                                                             \
    if (!pred(vm_stack_peek(vm, 0)) || !pred(vm_stack_peek(vm, 1))) {                              \
      vm_runtime_error(vm, "Operands must be numbers.");                                           \
      return INTERPRET_RUNTIME_ERROR;                                                              \
    }                                                                                              \
    double b = cast(vm_stack_pop(vm));                                                             \
    double a = cast(vm_stack_pop(vm));                                                             \
    vm_stack_push(vm, value_type(a op b));                                                         \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf(" ");
    for (Value *slot = vm->stack; slot < vm->stack_top; slot++) {
      printf("[ ");
      mesche_value_print(*slot);
      printf(" ]");
    }
    printf("\n");

    mesche_disasm_instr(&frame->closure->function->chunk,
                        (int)(frame->ip - frame->closure->function->chunk.code));
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
    case OP_NIL:
      vm_stack_push(vm, NIL_VAL);
      break;
    case OP_T:
      vm_stack_push(vm, T_VAL);
      break;
    case OP_POP:
      vm_stack_pop(vm);
      break;
    case OP_POP_SCOPE: {
      // Only start popping if we have locals to clear
      uint8_t local_count = READ_BYTE();
      if (local_count > 0) {
        Value result = vm_stack_pop(vm);
        for (int i = 0; i < local_count; i++) {
          vm_stack_pop(vm);
        }
        vm_stack_push(vm, result);
      }
      break;
    }
    case OP_ADD:
      BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, +);
      break;
    case OP_SUBTRACT:
      BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, -);
      break;
    case OP_MULTIPLY:
      BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, *);
      break;
    case OP_DIVIDE:
      BINARY_OP(NUMBER_VAL, IS_NUMBER, AS_NUMBER, /);
      break;
    case OP_AND:
      BINARY_OP(BOOL_VAL, IS_ANY, AS_BOOL, &&);
      break;
    case OP_OR:
      BINARY_OP(BOOL_VAL, IS_ANY, AS_BOOL, ||);
      break;
    case OP_NOT:
      vm_stack_push(vm, IS_NIL(vm_stack_pop(vm)) ? T_VAL : NIL_VAL);
      break;
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
      vm->frame_count--;

      // Close out upvalues for any function locals that have been captured by
      // closures
      vm_close_upvalues(vm, frame->slots);

      // If we're out of call frames, end execution
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
    case OP_READ_UPVALUE:
      // TODO: The problem here is that the local is no longer on the stack!
      // I'm using POP_SCOPE to get rid of all the locals but maybe the pointer
      // no longer works?  It's pointing to the location of the result of the function
      slot = READ_BYTE();
      vm_stack_push(vm, *frame->closure->upvalues[slot]->location);
      break;
    case OP_READ_LOCAL:
      slot = READ_BYTE();
      vm_stack_push(vm, frame->slots[slot]);
      break;
    case OP_SET_GLOBAL:
      name = READ_STRING();
      if (mesche_table_set(&vm->globals, name, vm_stack_peek(vm, 0))) {
        vm_runtime_error(vm, "Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      /* vm_stack_pop(vm); */
      break;
    case OP_SET_UPVALUE:
      slot = READ_BYTE();
      *frame->closure->upvalues[slot]->location = vm_stack_peek(vm, 0);
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
    case OP_CLOSURE: {
      ObjectFunction *function = AS_FUNCTION(READ_CONSTANT());
      ObjectClosure *closure = mesche_object_make_closure(vm, function);
      vm_stack_push(vm, OBJECT_VAL(closure));

      for (int i = 0; i < closure->upvalue_count; i++) {
        // If the upvalue is a local, capture it explicitly.  Otherwise,
        // grab a handle to the parent upvalue we're pointing to.
        uint8_t is_local = READ_BYTE();
        uint8_t index = READ_BYTE();

        if (is_local) {
          closure->upvalues[i] = vm_capture_upvalue(vm, frame->slots + index);
        } else {
          closure->upvalues[i] = frame->closure->upvalues[index];
        }
      }

      break;
    }
    case OP_CLOSE_UPVALUE:
      // NOTE: This opcode gets issued when a scope block is ending (usually
      // from a `let` or `begin` expression with multiple body expressions)
      // so skip the topmost value on the stack (the last expression result)
      // and grab the next value which should be the first local.
      vm_close_upvalues(vm, vm->stack_top - 2);

      // Move the result value into the local's spot
      Value result = vm_stack_pop(vm);
      vm_stack_pop(vm);
      vm_stack_push(vm, result);

      break;
    }

    // For now, we enforce that all instructions except OP_POP should produce
    // (or leave) a value on the stack so that we can be consistent with how
    // expressions are popped in blocks when their values aren't consumed.
    // TODO: Rethink this check, it doesn't work for binary operations which
    // pop 2 values from the stack
    /* if (instr != OP_POP && vm->stack_top < prev_stack_top) { */
    /*   PANIC("Instruction \"%d\" consumed a stack value! (before: %x, after: %x)\n", instr,
     * prev_stack_top, vm->stack_top); */
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

  // Push the top-level function as a closure
  vm_stack_push(vm, OBJECT_VAL(function));
  ObjectClosure *closure = mesche_object_make_closure(vm, function);
  vm_stack_pop(vm);
  vm_stack_push(vm, OBJECT_VAL(closure));

  // Call the initial closure
  vm_call(vm, closure, 0);

  // Define core native functions
  mesche_vm_define_native(vm, "clock", mesche_vm_clock_native);

  // Run the VM starting at the first call frame
  return mesche_vm_run(vm);
}
