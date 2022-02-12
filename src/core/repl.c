#include <flux.h>
#include <stdio.h>
#include <mesche.h>

void flux_repl_start(FILE *fp) {
  char input_buffer[2048];
  input_buffer[0] = '\0';

  VM vm;
  mesche_vm_init(&vm);

  printf("Welcome to the Flux Compose REPL!\n\n");

  while (!feof(fp)) {
    printf("> ");
    if (fgets(input_buffer, sizeof(input_buffer), fp) == NULL) {
      mesche_vm_free(&vm);
      PANIC("Error in fgets");
    }

    /* flux_log("EXPR: %s\n", input_buffer); */

    // TODO: Print the value for true REPL
    mesche_vm_eval_string(&vm, input_buffer);
    input_buffer[0] = '\0';
  }
}

void flux_repl_start_stdin(void) {
  flux_repl_start(stdin);
}

void flux_repl_start_socket(void) {
}
