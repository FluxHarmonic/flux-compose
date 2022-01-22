#include <flux.h>
#include <stdio.h>

#define INPUT_BUFFER_LIMIT 2048

void flux_repl_start(FILE *fp) {
  char input_buffer[INPUT_BUFFER_LIMIT];
  input_buffer[0] = '\0';

  printf("Welcome to the Flux Compose REPL!\n\n");

  while (!feof(fp)) {
    printf("> ");
    fgets(input_buffer, INPUT_BUFFER_LIMIT, fp);
    flux_log("EXPR: %s\n", input_buffer);

    // TODO: Print the value for true REPL
    flux_script_eval_string(input_buffer);
    input_buffer[0] = '\0';
  }
}

void flux_repl_start_stdin(void) {
  flux_repl_start(stdin);
}

void flux_repl_start_socket(void) {
}
