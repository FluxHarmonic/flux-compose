#ifndef mesche_mem_h
#define mesche_mem_h

#include <stdio.h>
#include <string.h>

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2);
#define GROW_ARRAY(type, pointer, old_size, new_size) \
  (type*)mesche_mem_realloc(pointer, sizeof(type) * old_size, sizeof(type) * new_size)
  /* flux_log("GROW_ARRAY for type %s at %d\n", #type, __LINE__); */

#define FREE(type, pointer) mesche_mem_realloc(pointer, sizeof(type), 0);
#define FREE_ARRAY(type, pointer, size) \
  mesche_mem_realloc((pointer), sizeof(type) * size, 0)
#define FREE_SIZE(pointer, size) \
  mesche_mem_realloc((pointer), size, 0)

void *mesche_mem_realloc(void *mem_ptr, size_t old_size, size_t new_size);

#endif
