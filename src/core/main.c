#include <stdio.h>
#include <stdlib.h>
#include "../../lib/flux.h"

int main(int argc, char **argv) {
  FILE *script_file = flux_file_open(
      "/home/daviwil/Projects/Code/flux-compose/examples/ideas.flx", "r");

  if (script_file == NULL) {
    printf("Could not load script file!\n");
    exit(1);
  }

  flux_script_eval(script_file);

  return 0; /* never reached, see inner_main */
}
