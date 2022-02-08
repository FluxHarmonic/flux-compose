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

char *flux_file_read_all(const char *file_path) {
  FILE *file = fopen(file_path, "rb");

  if (file == NULL) {
    PANIC("Could not open file \"%s\"!\n", file_path);
  }

  // Seek to the end to figure out how big of a char buffer we need
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  rewind(file);

  // Allocate the buffer and read the file contents into it
  char *buffer = (char*)malloc(file_size + 1);
  if (buffer == NULL) {
    PANIC("Could not allocate enough memory to read file \"%s\"!\n", file_path);
  }

  size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
  if (bytes_read < file_size) {
    PANIC("Could not read contents of file \"%s\"!\n", file_path);
  }

  // Finish off the buffer with a null terminator
  buffer[bytes_read] = '\0';
  fclose(file);

  return buffer;
}
