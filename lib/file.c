#include <flux.h>
#include <stdio.h>
#include <string.h>

FILE *flux_file_open(const char *file_name, const char *mode_string) {
  // TODO: Resolve relative file paths
  // TODO: Check errno, log it
  return fopen(file_name, mode_string);
}

FILE *flux_file_from_string(const char *file_contents) {
  // Open a stream for the string contents
  return fmemopen((void *)file_contents, strlen(file_contents), "r");
}
