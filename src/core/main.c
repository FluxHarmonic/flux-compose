#include "app.h"
#include <flux.h>
#include <mesche.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    VM vm;
    mesche_vm_init(&vm);

    // Register some native functions
    mesche_vm_define_native(&vm, "show-preview-window", flux_graphics_func_show_preview_window);
    mesche_vm_define_native(&vm, "render-to-file", flux_graphics_func_render_to_file);
    mesche_vm_define_native(&vm, "flux-harmonic-thumbnail",
                            flux_graphics_func_flux_harmonic_thumbnail);

    mesche_vm_eval_string(&vm, script_source);
    printf("\n");

    // Report the final memory allocation statistics
    mesche_mem_report((MescheMemory*)&vm);

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
