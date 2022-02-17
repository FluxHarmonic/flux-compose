#ifndef mesche_mem_h
#define mesche_mem_h

#include <stdio.h>
#include <string.h>

// If this is defined, the GC will be run frequently
/* #define DEBUG_STRESS_GC */

// Log when garbage collection occurs
/* #define DEBUG_LOG_GC */

struct MescheMemory;

// Stores a pointer to the garbage collector (almost certainly from the VM)
typedef void (*MescheMemoryCollectGarbageFunc)(struct MescheMemory *);

// Contains pointers to objects which assist with memory management
// and object usage tracking.
typedef struct MescheMemory {
  MescheMemoryCollectGarbageFunc collect_garbage_func;
  size_t bytes_allocated;
  size_t next_gc;
} MescheMemory;

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2);
#define GROW_ARRAY(mem, type, pointer, old_size, new_size)                \
  (type*)mesche_mem_realloc(mem, pointer, sizeof(type) * old_size, sizeof(type) * new_size)
  /* flux_log("GROW_ARRAY for type %s at %d\n", #type, __LINE__); */

#define FREE(mem, type, pointer) mesche_mem_realloc((MescheMemory *)mem, pointer, sizeof(type), 0);
#define FREE_ARRAY(mem, type, pointer, size)              \
  mesche_mem_realloc((MescheMemory *)mem, (pointer), sizeof(type) * size, 0)
#define FREE_SIZE(mem, pointer, size)             \
  mesche_mem_realloc((MescheMemory *)mem, (pointer), size, 0)

void mesche_mem_init(MescheMemory *mem, MescheMemoryCollectGarbageFunc collect_garbage_func);
void *mesche_mem_realloc(MescheMemory *mem, void *mem_ptr, size_t old_size, size_t new_size);
void mesche_mem_collect_garbage(MescheMemory *mem);
void mesche_mem_report(MescheMemory *mem);

#endif
