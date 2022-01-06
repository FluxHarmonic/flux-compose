#include <stdio.h>
#include <stdarg.h>

FILE *log_file = NULL;

void open_log(const char *file_path) {
  if (log_file == NULL) {
    log_file = fopen(file_path, "w");
  }
}

void flux_log(const char *format, ...) {
  if (log_file != NULL) {
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fflush(log_file);
  }
}
