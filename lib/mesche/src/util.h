#ifndef mesche_util_h
#define mesche_util_h

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define PANIC(message, ...)                                                                        \
  fprintf(stderr, "\n\e[1;31mPANIC\e[0m in \e[0;93m%s\e[0m: ", __func__); \
  fprintf(stderr, message, ##__VA_ARGS__);                               \
  exit(1);

#endif
