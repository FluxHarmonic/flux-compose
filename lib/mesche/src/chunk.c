#include <stdio.h>

#include "vm.h"
#include "mem.h"
#include "chunk.h"
#include "value.h"

void mesche_chunk_init(Chunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  mesche_value_array_init(&chunk->constants);
}

void mesche_chunk_write(MescheMemory *mem, Chunk* chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int old_capacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(old_capacity);
    chunk->code = GROW_ARRAY(mem, uint8_t, chunk->code, old_capacity, chunk->capacity);
    chunk->lines = GROW_ARRAY(mem, int, chunk->lines, old_capacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

int mesche_chunk_constant_add(MescheMemory *mem, Chunk *chunk, Value value) {
  // TODO: This breaks the abstraction!  Necessary for now, though...
  VM *vm = (VM*)mem;

  // Make sure the constant is on the value stack before pushing
  // so that it gets marked correctly in a GC pass
  mesche_vm_stack_push(vm, value);
  mesche_value_array_write(mem, &chunk->constants, value);
  mesche_vm_stack_pop(vm);
  return chunk->constants.count - 1;
}

void mesche_chunk_free(MescheMemory *mem, Chunk* chunk) {
  FREE_ARRAY(mem, uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(mem, uint8_t, chunk->lines, chunk->capacity);
  mesche_value_array_free(mem, &chunk->constants);
  mesche_chunk_init(chunk);
}
