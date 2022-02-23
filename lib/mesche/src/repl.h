#ifndef mesche_repl_h
#define mesche_repl_h

#include <stdio.h>
#include <stdbool.h>

#include "vm.h"

typedef struct {
  VM *vm;
  int fd;
  int input_length;
  bool is_async;
  char input_buffer[2048];
} MescheRepl;

int mesche_repl_poll(MescheRepl *repl);
MescheRepl *mesche_repl_start_async(VM *vm, FILE *fp);
void mesche_repl_start(VM *vm, FILE *fp);

#endif
