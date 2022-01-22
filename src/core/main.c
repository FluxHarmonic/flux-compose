#include "app.h"
#include <flux.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  /* FILE *script_file = flux_file_open( */
  /*     "/home/daviwil/Projects/Code/flux-compose/examples/basic-gfx.fxs",
   * "r"); */

  FILE *script_file = flux_file_from_string("(show-preview-window 1280 720)\n");

  if (script_file == NULL) {
    printf("Could not load script file!\n");
    exit(1);
  }

  /* flux_script_eval(script_file); */

  flux_repl_start_stdin();

  return 0; /* never reached, see inner_main */
}
