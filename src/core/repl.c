#include <flux.h>
#include <stdio.h>
#include <mesche.h>

void flux_repl_start(FILE *fp) {
  char input_buffer[2048];
  input_buffer[0] = '\0';

  VM vm;
  mesche_vm_init(&vm);

  mesche_module_enter_by_name(&vm, "flux graphics");
  mesche_vm_define_native(&vm, "show-preview-window", flux_graphics_func_show_preview_window, true);
  mesche_vm_define_native(&vm, "render-to-file", flux_graphics_func_render_to_file, true);
  mesche_module_enter(&vm, vm.root_module);

  printf("Welcome to the Flux Compose REPL!\n\n");

  while (!feof(fp)) {
    printf("\e[1;92mmesche:\e[0m");
    mesche_module_print_name(vm.current_module);
    printf("> ");
    if (fgets(input_buffer, sizeof(input_buffer), fp) == NULL) {
      mesche_vm_free(&vm);
      PANIC("Error in fgets");
    }

    /* flux_log("EXPR: %s\n", input_buffer); */

    // Evaluate the string
    mesche_vm_eval_string(&vm, input_buffer);
    input_buffer[0] = '\0';

    // Print the last remaining value on the stack
    Value result = mesche_vm_stack_pop(&vm);
    mesche_value_print(result);
    printf("\n");
  }
}

void flux_repl_start_stdin(void) {
  flux_repl_start(stdin);
}

void flux_repl_start_socket(void) {
}
