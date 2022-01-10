#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

void *flux_memory_alloc(size_t size) {
  // TODO: Conditionally log it
  return malloc(size);
}

void flux_memory_dealloc(void *mem_ptr) {
  // TODO: Conditionally log it
  free(mem_ptr);
}
