#include <stdio.h>

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

void mesche_chunk_write(Chunk* chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int old_capacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(old_capacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
    chunk->lines = GROW_ARRAY(int, chunk->lines, old_capacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

int mesche_chunk_constant_add(Chunk *chunk, Value value) {
  mesche_value_array_write(&chunk->constants, value);
  return chunk->constants.count - 1;
}

void mesche_chunk_free(Chunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(uint8_t, chunk->lines, chunk->capacity);
  mesche_value_array_free(&chunk->constants);
  mesche_chunk_init(chunk);
}
