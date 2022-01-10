#include <stdio.h>
#include "flux.h"

FILE *flux_file_open(char *file_name, char *mode_string) {
  // TODO: Resolve relative file paths
  // TODO: Check errno, log it
  return fopen(file_name, mode_string);
}

FILE *flux_file_from_string(char *file_contents) {
  // Open a stream for the string contents
  return fmemopen(file_contents, strlen(file_contents), "r");
}
