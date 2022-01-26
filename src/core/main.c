#include "app.h"
#include <flux.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  /* FILE *script_file = flux_file_open( */
  /*     "/home/daviwil/Projects/Code/flux-compose/examples/basic-gfx.fxs",
   * "r"); */

  /* FILE *script_file = flux_file_from_string("(show-preview-window 1280 720)\n"); */

  /* if (script_file == NULL) { */
  /*   printf("Could not load script file!\n"); */
  /*   exit(1); */
  /* } */

  /* flux_repl_start_stdin(); */

  flux_graphics_init();
  FluxWindow window = flux_graphics_window_create(1280, 720, "Flux Compose");
  flux_graphics_window_show(window);

  // Start the loop and wait until it finishes
  flux_graphics_loop_start(window);
  flux_graphics_loop_wait();

  flux_graphics_window_destroy(window);
  flux_graphics_end();

  return 0;
}
