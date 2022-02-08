#ifndef mesche_chunk_h
#define mesche_chunk_h

#include <stdint.h>
#include "value.h"

typedef struct {
  int capacity;
  int count;
  uint8_t *code;
  int *lines;
  ValueArray constants;
} Chunk;

void mesche_chunk_init(Chunk* chunk);
void mesche_chunk_write(Chunk* chunk, uint8_t byte, int line);
int mesche_chunk_constant_add(Chunk *chunk, Value value);
void mesche_chunk_free(Chunk* chunk);

#endif
