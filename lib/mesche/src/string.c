#include <stdlib.h>
#include "string.h"

ObjectString *mesche_string_join(VM *vm, ObjectString *left, ObjectString *right, const char *separator) {
  size_t separator_len = 0;
  size_t new_length = left->length + right->length;
  if (separator != NULL) {
    separator_len = strlen(separator);
    new_length += separator_len;
  }

  // Allocate a temporary buffer into which we'll copy the string parts
  char *string_buffer = malloc(sizeof(char) * new_length);
  char *copy_ptr = string_buffer;

  // Copy the requisite string segments into the new buffer
  memcpy(copy_ptr, left->chars, left->length);
  copy_ptr += left->length;
  if (separator) {
    memcpy(copy_ptr, separator, separator_len);
    copy_ptr += separator_len;
  }
  memcpy(copy_ptr, right->chars, right->length);

  // Allocate the new string, free the temporary buffer, and return
  ObjectString *new_string = mesche_object_make_string(vm, string_buffer, new_length);
  free(string_buffer);
  return new_string;
}
