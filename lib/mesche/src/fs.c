#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fs.h"
#include "util.h"

bool mesche_fs_path_exists_p(const char *fs_path) {
  return access(fs_path, F_OK) != -1;
}

bool mesche_fs_path_absolute_p(const char *fs_path) {
  return fs_path[0] == '/';
}

char *mesche_fs_resolve_path(const char *fs_path) {
  char *current_path = getcwd(NULL, 0);
  if (current_path != NULL) {
    size_t size = strlen(current_path) + strlen(fs_path) + 2;
    char *joined_path = malloc(size);
    snprintf(joined_path, size, "%s/%s", current_path, fs_path);

    // Resolve the joined path and free the intermediate strings
    char *resolved_path = realpath(joined_path, NULL);
    free(current_path);
    free(joined_path);
    return resolved_path;
  } else {
    PANIC("Error while calling getcwd()");
  }
}

char *mesche_fs_file_read_all(const char *file_path) {
  FILE *file = fopen(file_path, "rb");

  // TODO: Use a less catastrophic error report
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
