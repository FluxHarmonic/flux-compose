#ifndef mesche_table_h
#define mesche_table_h

#include <stdint.h>
#include "mem.h"
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
void mesche_table_free(MescheMemory *mem, Table *table);

bool mesche_table_set(MescheMemory *mem, Table *table, ObjectString *key, Value value);
bool mesche_table_get(Table *table, ObjectString *key, Value *value);
void mesche_table_copy(MescheMemory *mem, Table *from, Table *to);
bool mesche_table_delete(Table *table, ObjectString *key);
ObjectString *mesche_table_find_key(Table *table, const char *chars, int length, uint32_t hash);

#endif
