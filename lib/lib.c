#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <flux-internal.h>
#include <flux.h>

ValueHeader *flux_graphics_func_show_preview_window(VectorCursor *list_cursor,
                                                    ValueCursor *value_cursor) {
  flux_log("ATTEMPTING TO LOAD WINDOW\n");

  // TODO: Pass actual width and height
  /* init_graphics(1280, 720); */

  // TODO: Is this appropriate
  return NULL;
}
