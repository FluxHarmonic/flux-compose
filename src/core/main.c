#include "app.h"
#include <flux.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <mesche.h>

int main(int argc, char **argv) {
  bool use_repl = false;
  const char *script_source = NULL;

  // Check program arguments
  if (argc > 1) {
    if (strcmp(argv[1], "--repl") == 0) {
      use_repl = true;
    } else {
      // Treat it as a file path
      script_source = flux_file_read_all(argv[1]);
      if (script_source == NULL) {
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
  } else if (script_source != NULL) {
    // Evaluate the script
    //flux_script_eval(script_file);
    /* flux_script_compile(script_file); */
    VM vm;
    mesche_vm_init(&vm);
    mesche_vm_eval_string(&vm, script_source);
    printf("\n");

    // Free the VM and allocated source string
    mesche_vm_free(&vm);
    free((void *)script_source);

    // Wait for the graphics loop to complete
    flux_graphics_loop_wait();
  }

  // TODO: Destroy window properly
  /* flux_graphics_window_destroy(window); */
  flux_graphics_end();

  return 0;
}
