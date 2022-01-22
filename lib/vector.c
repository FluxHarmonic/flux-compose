#include <flux-internal.h>
#include <flux.h>
#include <stdio.h>

// Initialize a Vector struct at a pre-existing location.
//
// The `item_size_func` parameter is a pointer to a function that will report
// the size for an item at a specific location in vector memory.
//
// The structs used for vector items must use an enum for the item kind as
// the first field for each struct that that will be stored in the vector.
Vector flux_vector_init(Vector vector, VectorItemSizeFunc item_size_func) {
  vector->start_item = vector + 1;
  // TODO: Need a way to abstract buffer management!
  vector->buffer_size = -1;
  vector->item_size_func = item_size_func;
  flux_vector_reset(vector);

  return vector;
}

// Create a new vector, preallocating memory of the specified size.
//
// The `item_size_func` parameter is a pointer to a function that will report
// the size for an item at a specific location in vector memory.
//
// The structs used for vector items must use an enum for the item kind as
// the first field for each struct that that will be stored in the vector.
Vector flux_vector_create(size_t initial_size, VectorItemSizeFunc item_size_func) {
  Vector vector = flux_memory_alloc(initial_size);
  flux_vector_init(vector, item_size_func);
  vector->buffer_size = initial_size;
  return vector;
}

void flux_vector_cursor_init(Vector vector, VectorCursor *cursor) {
  cursor->index = -1;
  cursor->vector = vector;
  cursor->current_item = NULL;
}

// Gets the next item in the specified vector starting from the given cursor.
void *flux_vector_cursor_next(VectorCursor *cursor) {
  if (cursor->current_item == NULL) {
    cursor->index = 0;
    cursor->current_item = cursor->vector->start_item;
  } else if (cursor->index + 1 >= cursor->vector->length) {
    // No more items!
    return NULL;
  } else {
    // TODO: What happens when you've reached the end?
    cursor->current_item = (void *)((uintptr_t)cursor->current_item +
                                    cursor->vector->item_size_func(cursor->current_item));
    cursor->index++;
  }

  return cursor->current_item;
}

// Returns 1 if the vector cursor can move forward or 0 otherwise.
char flux_vector_cursor_has_next(VectorCursor *cursor) {
  return cursor->vector->length > 0 && cursor->index + 1 < cursor->vector->length;
}

// Adds a new item to the vector by copying the memory at the specified location.
void *flux_vector_push(VectorCursor *cursor, void *item) {
  void *current = NULL;
  size_t item_size;

  // Initialize the cursor if it hasn't been yet
  // TODO: Error in this case instead?
  if (cursor->current_item == NULL) {
    cursor->current_item = cursor->vector->start_item;
    cursor->index = -1; // We're about to increment it
  }

  // Copy the contents of item
  item_size = cursor->vector->item_size_func(item);
  memcpy(cursor->current_item, item, item_size);

  // TODO: Resize when needed
  current = cursor->current_item;
  cursor->index++;
  cursor->vector->length++;
  cursor->vector->buffer_usage += item_size;
  cursor->current_item = (void *)((uintptr_t)cursor->current_item + (uintptr_t)item_size);
  *(int *)cursor->current_item = 0; // Set the 'kind' of the next item to 0

  return current;
}

// Resets the internal vector state to appear empty
void flux_vector_reset(Vector vector) {
  vector->length = 0;
  vector->buffer_usage = 0;
  *(int *)vector->start_item = 0; // Set the 'kind' of the first item to 0
}
