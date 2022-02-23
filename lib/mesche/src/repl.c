#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "repl.h"
#include "util.h"
#include "module.h"

static void mesche_repl_print_prompt(VM *vm) {
  printf("\e[1;92mmesche:\e[0m");
  mesche_module_print_name(vm->current_module);
  printf("> ");
  fflush(stdout);
}

int mesche_repl_poll(MescheRepl *repl) {
  // Read as many characters as possible in one poll
  while (true) {
    int result = read(repl->fd, repl->input_buffer + repl->input_length, 1);
    if (result == 0) {
      // End of file, exit
      return -1;
    } else if (result == -1 && errno == EAGAIN) {
      // Nothing to read this pass
      return 0;
    } else if (result > 0) {
      repl->input_length += result;
      repl->input_buffer[repl->input_length] = '\0';

      if (repl->input_buffer[repl->input_length - 1] == '\n') {
        printf("Evaluate string: %s\n", repl->input_buffer);

        // Evaluate the string
        mesche_vm_eval_string(repl->vm, repl->input_buffer);
        repl->input_length = 0;
        repl->input_buffer[0] = '\0';

        // Print the last remaining value on the stack
        Value result = mesche_vm_stack_pop(repl->vm);
        mesche_value_print(result);
        printf("\n");

        // Print the next prompt
        mesche_repl_print_prompt(repl->vm);
      }
    }
  }
}

MescheRepl *mesche_repl_start_async(VM *vm, FILE *fp) {
  // First, try to set the file descriptor to be non-blocking
  int fd = fileno(fp);
  int result = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  if (result == -1) {
    PANIC("fcntl failed with errno %d\n", errno);
  }

  MescheRepl *repl = malloc(sizeof(MescheRepl));
  repl->vm = vm;
  repl->fd = fd;
  repl->is_async = true;
  repl->input_buffer[0] = '\0';

  mesche_repl_print_prompt(vm);

  return repl;
}

void mesche_repl_start(VM *vm, FILE *fp) {
  char input_buffer[2048];
  input_buffer[0] = '\0';

  printf("Mesche REPL\n\n");

  while (!feof(fp)) {
    printf("\e[1;92mmesche:\e[0m");
    mesche_module_print_name(vm->current_module);
    printf("> ");
    if (fgets(input_buffer, sizeof(input_buffer), fp) == NULL) {
      PANIC("Error in fgets");
    }

    // Evaluate the string
    mesche_vm_eval_string(vm, input_buffer);
    input_buffer[0] = '\0';

    // Print the last remaining value on the stack
    Value result = mesche_vm_stack_pop(vm);
    mesche_value_print(result);
    printf("\n");
  }
}
