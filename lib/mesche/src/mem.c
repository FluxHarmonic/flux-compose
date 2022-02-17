#include <stdlib.h>
#include <stdio.h>

#include "util.h"
#include "mem.h"

// This is arbitrarily picked, inspired by Crafting Interpreters.
// It will probably need to be adjusted over time!
#define GC_INITIAL_LIMIT 1024 * 1024;

// The factor by which the heap limit will be extended after a sweep
#define GC_HEAP_GROW_FACTOR 2;

void mesche_mem_init(MescheMemory *mem, MescheMemoryCollectGarbageFunc collect_garbage_func) {
  mem->collect_garbage_func = collect_garbage_func;
  mem->bytes_allocated = 0;
  mem->next_gc = GC_INITIAL_LIMIT;
}

void *mesche_mem_realloc(MescheMemory *mem, void *mem_ptr, size_t old_size, size_t new_size) {
  // Adjust the memory allocation amount
  mem->bytes_allocated += new_size - old_size;

  // Decide whether to collect garbage
  if (new_size > old_size) {
    #ifdef DEBUG_STRESS_GC
    mesche_mem_collect_garbage(mem);
    #else
    if (mem->bytes_allocated > mem->next_gc) {
      mesche_mem_collect_garbage(mem);
    }
    #endif
  }

  if (new_size == 0) {
    free(mem_ptr);
    return NULL;
  }

  void *new_ptr = realloc(mem_ptr, new_size);
  if (new_ptr == NULL) {
    // Reallocation failed, bail out
    // TODO: Is there a more specific error code I can check?
    PANIC("Memory reallocation failed!\n");
  }

  return new_ptr;
}

void mesche_mem_collect_garbage(MescheMemory *mem) {
  if (mem->collect_garbage_func == NULL) {
    PANIC("No garbage collector function is registered.");
  }

  #ifdef DEBUG_LOG_GC
  printf("-- GC starting...\n");
  size_t before_size = mem->bytes_allocated;
  #endif

  // Collect garbage and adjust the next GC limit
  mem->collect_garbage_func(mem);
  mem->next_gc = mem->bytes_allocated * GC_HEAP_GROW_FACTOR;

  #ifdef DEBUG_LOG_GC
  printf("-- GC finished: freed %zu bytes (from %zu to %zu), next GC at %zu bytes\n",
         before_size - mem->bytes_allocated,
         before_size,
         mem->bytes_allocated,
         mem->next_gc);
  #endif
}

void mesche_mem_report(MescheMemory *mem) {
  printf("-- %zu bytes allocated in memory, next GC at %zu bytes\n", mem->bytes_allocated, mem->next_gc);
}
