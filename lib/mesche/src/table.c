#include <stdio.h>

#include "mem.h"
#include "table.h"
#include "object.h"

#define TABLE_MAX_LOAD 0.75

void mesche_table_init(Table *table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void mesche_table_free(Table *table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
}

static Entry *table_find_entry(Entry *entries, int capacity, ObjectString *key) {
  Entry *tombstone = NULL;
  uint32_t index = key->hash % capacity;

  for (;;) {
    Entry *entry = &entries[index];

    // If the key is empty but there is a 't' value, it's a tombstone
    if (entry->key == NULL) {
      // Check for tombstones (removed entries)
      if (!IS_T(entry->value)) {
        // Return the found tombstone slot, otherwise this entry
        return tombstone != NULL ? tombstone : entry;
      } else {
        // Found a tombstone, store it
        if (tombstone == NULL) {
          tombstone = entry;
        }
      }
    } else if (entry->key == key) {
      // Found the key
      return entry;
    }

    index = (index + 1) % capacity;
  }
}

static void table_adjust_capacity(Table *table, int capacity) {
  // Allocate and initialize the new array
  Entry *entries = mesche_mem_realloc(NULL, 0, sizeof(Entry) * capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  // Copy over existing table entries
  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    if (entry->key == NULL) {
      continue;
    }

    Entry *dest = table_find_entry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;

    // Recount the table entries to not include tombstone count
    table->count++;
  }

  // Free the old table array
  FREE_ARRAY(Entry, table->entries, table->capacity);

  table->entries = entries;
  table->capacity = capacity;
}

bool mesche_table_set(Table *table, ObjectString *key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    table_adjust_capacity(table, capacity);
  }

  Entry *entry = table_find_entry(table->entries, table->capacity, key);
  bool is_new_key = entry->key == NULL;
  if (is_new_key && IS_NIL(entry->value)) {
    // Only increase the count if the returned entry is nil,
    // meaning that it wasn't a tombstoned entry
    table->count++;
  }

  entry->key = key;
  entry->value = value;

  return is_new_key;
}

bool mesche_table_get(Table *table, ObjectString *key, Value *value) {
  if (table->count == 0) return false;

  Entry *entry = table_find_entry(table->entries, table->capacity, key);
  if (entry->key != NULL) return false;

  *value = entry->value;
  return true;
}

bool mesche_table_delete(Table *table, ObjectString *key) {
  if (table->count == 0) return false;

  // Find the existing entry
  Entry *entry = table_find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  // Set a tombstone value at this entry
  entry->key = NULL;
  entry->value = T_VAL;

  return true;
}

void mesche_table_copy(Table *from, Table *to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry *entry = &from->entries[i];
    if (entry->key != NULL) {
      mesche_table_set(to, entry->key, entry->value);
    }
  }
}

ObjectString *mesche_table_find_key(Table *table, const char *chars, int length, uint32_t hash) {
  if (table->count == 0) return NULL;

  // Use a similar algorithm as normal value lookup, but streamlined for key search
  uint32_t index = hash % table->capacity;
  for (;;) {
    Entry *entry = &table->entries[index];
    if (entry->key == NULL) {
      // Is this a tombstoned entry?
      if (IS_NIL(entry->value)) {
        // The expected location is empty and non-tombstoned,
        // so the string does not exist in the table
        return NULL;
      }
    } else if (entry->key->length == length
               && entry->key->hash == hash
               && memcmp(entry->key->chars, chars, length) == 0) {
      // Return the existing key
      return entry->key;
    }

    index = (index + 1) % table->capacity;
  }
}
