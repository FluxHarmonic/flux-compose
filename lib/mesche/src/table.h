#ifndef mesche_table_h
#define mesche_table_h

#include <stdint.h>
#include "value.h"

typedef struct {
  ObjectString *key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry *entries;
} Table;

void mesche_table_init(Table *table);
void mesche_table_free(Table *table);

bool mesche_table_set(Table *table, ObjectString *key, Value value);
bool mesche_table_get(Table *table, ObjectString *key, Value *value);
void mesche_table_copy(Table *from, Table *to);
ObjectString *mesche_table_find_key(Table *table, char *chars, int length, uint32_t hash);

#endif
