#include "vm.h"
#include "compiler.h"

InterpretResult mesche_eval_string(VM *vm, const char *script_string) {
  // Compile the source into a fresh chunk
  Chunk chunk;
  mesche_chunk_init(&chunk);
  if (!mesche_compile_source(vm, script_string, &chunk)) {
    mesche_chunk_free(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  // Evaluate the chunk in the VM
  vm->chunk = &chunk;
  vm->ip = vm->chunk->code;
  InterpretResult result = mesche_vm_run(vm);

  // Free the memory chunk and return the result
  mesche_chunk_free(&chunk);
  return result;
}
