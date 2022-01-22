#include "../lib/flux-internal.h"
#include "../lib/flux.h"
#include "test.h"

Vector test_vector = NULL;

typedef enum {
  TestKindNone,
  TestKindFixed,
  TestKindVariable,
} TestKind;

typedef struct {
  TestKind kind;
} TestHeader;

typedef struct {
  TestHeader header;
  int x;
  int y;
} TestFixed;

typedef struct {
  TestHeader header;
  int length;
  char name[32];
} TestVariable;

size_t test_kind_size(void *item) {
  switch (((TestHeader *)item)->kind) {
  case TestKindFixed:
    return sizeof(TestFixed);
  case TestKindVariable:
    return sizeof(TestVariable) + ((TestVariable *)item)->length + 1;
  default:
    PANIC("Unexpected test item kind: %d", ((TestHeader *)item)->kind);
  }
}

void test_vector_empty(void) {
  ASSERT_INT(0, test_vector->length);
  ASSERT_INT(512, test_vector->buffer_size);

  VectorCursor cursor;
  flux_vector_cursor_init(test_vector, &cursor);
  ASSERT_INT(0, flux_vector_cursor_has_next(&cursor));

  PASS();
}

void test_vector_push(void) {
  VectorCursor cursor;
  flux_vector_cursor_init(test_vector, &cursor);

  TestFixed fixed1;
  fixed1.header.kind = TestKindFixed;
  fixed1.x = 50;
  fixed1.y = 40;
  flux_vector_push(&cursor, &fixed1);

  ASSERT_INT(0, cursor.index);
  ASSERT_INT(1, test_vector->length);

  TestVariable var;
  var.header.kind = TestKindVariable;
  var.length = 5;
  strcpy(var.name, "Test!");
  flux_vector_push(&cursor, &var);

  ASSERT_INT(1, cursor.index);
  ASSERT_INT(2, test_vector->length);

  TestFixed fixed2;
  fixed2.header.kind = TestKindFixed;
  fixed2.x = 200;
  fixed2.y = -10;
  flux_vector_push(&cursor, &fixed2);

  ASSERT_INT(2, cursor.index);
  ASSERT_INT(3, test_vector->length);

  PASS();
}

void test_vector_cursor(void) {
  VectorCursor cursor;
  flux_vector_cursor_init(test_vector, &cursor);

  TestFixed *fixed1 = flux_vector_cursor_next(&cursor);
  ASSERT_INT(0, cursor.index);
  ASSERT_INT(TestKindFixed, fixed1->header.kind);

  TestVariable *var = flux_vector_cursor_next(&cursor);
  ASSERT_INT(1, cursor.index);
  ASSERT_INT(TestKindVariable, var->header.kind);

  TestFixed *fixed2 = flux_vector_cursor_next(&cursor);
  ASSERT_INT(2, cursor.index);
  ASSERT_INT(TestKindFixed, fixed2->header.kind);

  // There should be an item with kind = 0 at the end
  ASSERT_INT(0, flux_vector_cursor_has_next(&cursor));
  TestHeader *last = flux_vector_cursor_next(&cursor);
  ASSERT_INT(NULL, last);

  PASS();
}

void test_vector_reset(void) {
  flux_vector_reset(test_vector);

  TestHeader *first = test_vector->start_item;
  ASSERT_INT(0, test_vector->length);
  ASSERT_INT(TestKindNone, first->kind);

  PASS();
}

void test_vector_suite(void) {
  SUITE();

  // Create the test vector first
  test_vector = flux_vector_create(512, &test_kind_size);

  test_vector_empty();
  test_vector_push();
  test_vector_cursor();
  test_vector_reset();

  // Clean up test memory
  if (test_vector != NULL) {
    free(test_vector);
  }
}
