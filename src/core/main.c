#include "app.h"
#include <flux.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

int main(int argc, char **argv) {
  bool use_repl = false;
  FILE *script_file = NULL;

  // Check program arguments
  if (argc > 1) {
    if (strcmp(argv[1], "--repl") == 0) {
      use_repl = true;
    } else {
      // Treat it as a file path
      script_file = flux_file_open(argv[1], "r");
      if (script_file == NULL) {
        printf("ERROR: Could not load script file: %s\n\n", argv[1]);
        exit(1);
      }
    }
  } else {
    printf("\nFlux Compose\n\n  Usage: flux-compose <--repl | path/to/file.fxs>\n\n");
    exit(0);
  }

  if (use_repl) {
    flux_repl_start_stdin();
  } else if (script_file != NULL) {
    // Evaluate the script
    flux_script_eval(script_file);

    // Wait for the graphics loop to complete
    flux_graphics_loop_wait();
  }

  // TODO: Destroy window properly
  /* flux_graphics_window_destroy(window); */
  flux_graphics_end();

  return 0;
}
