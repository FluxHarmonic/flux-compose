#include "app.h"
#include <flux.h>
#include <mesche.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  bool use_repl = false;
  const char *script_path = NULL;

  // Check program arguments
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "--repl") == 0) {
        use_repl = true;
      } else {
        // Treat it as a file path
        script_path = argv[1];
      }
    }
  } else {
    printf("\nFlux Compose\n\n  Usage: flux-compose <--repl | path/to/file.fxs>\n\n");
    exit(0);
  }

  VM vm;
  mesche_vm_init(&vm);

  // Set up load paths
  mesche_vm_load_path_add(&vm, "lib/mesche/modules/");

  // Register some native functions
  mesche_module_enter_by_name(&vm, "flux graphics");
  mesche_vm_define_native(&vm, "show-preview-window", flux_graphics_func_show_preview_window, true);
  mesche_vm_define_native(&vm, "render-to-file", flux_graphics_func_render_to_file, true);
  mesche_vm_define_native(&vm, "flux-harmonic-thumbnail",
                          flux_graphics_func_flux_harmonic_thumbnail, true);
  mesche_vm_define_native(&vm, "font-load-internal", flux_graphics_func_load_font_internal, false);
  mesche_vm_define_native(&vm, "image-load-internal", flux_texture_func_image_load_internal, false);
  mesche_vm_define_native(&vm, "scene-image-make", flux_scene_func_scene_image_make, false);
  mesche_vm_define_native(&vm, "scene-make", flux_scene_func_scene_make, false);
  mesche_vm_define_native(&vm, "graphics-scene-set!", flux_graphics_func_graphics_scene_set, false);

  // Initialize the graphics subsystem
  flux_graphics_init();
  FluxWindow window = flux_graphics_window_create(1280, 720, "Flux Compose");
  vm.app_context = window;

  MescheRepl *repl = NULL;
  if (use_repl) {
    // Start the REPL and show the preview window
    printf("Welcome to the Flux Compose REPL!\n\n");
    repl = mesche_repl_start_async(&vm, stdin);
    flux_graphics_window_show(window);
  }

  if (script_path != NULL) {
    // Evaluate the script before starting the renderer
    mesche_vm_load_file(&vm, script_path);
    printf("\n");
  }

  // Start the render loop
  flux_graphics_loop_start(window, repl);

  // Report the final memory allocation statistics
  mesche_mem_report((MescheMemory *)&vm);

  // Free the VM and allocated source string
  mesche_vm_free(&vm);

  // Destroy the window and shut down the renderer
  flux_graphics_window_destroy(window);
  flux_graphics_end();

  return 0;
}
