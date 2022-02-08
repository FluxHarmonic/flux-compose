#include <stdlib.h>

#include "util.h"
#include "mem.h"

void *mesche_mem_realloc(void *mem_ptr, size_t old_size, size_t new_size) {
  if (new_size == 0) {
    free(mem_ptr);
    return NULL;
  }

  /* flux_log("REALLOC: Requesting %d (previously %d)\n", new_size, old_size); */

  void *new_ptr = realloc(mem_ptr, new_size);
  if (new_ptr == NULL) {
    // Reallocation failed, bail out
    // TODO: Is there a more specific error code I can check?
    PANIC("Memory reallocation failed!\n");
  }

  return new_ptr;
}
