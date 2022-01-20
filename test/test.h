#ifndef __TEST_H
#define __TEST_H

#include <stdio.h>

extern unsigned int tests_passed;
extern unsigned int tests_skipped;
extern unsigned int tests_failed;

extern char fail_message[2048];

#define SUITE()                                                                \
  printf("\n\n\e[1;33m • SUITE\e[0m %s\n", __func__);

#define PASS()                                                                 \
  tests_passed++;                                                              \
  printf("\e[1;92m  ✓ PASS\e[0m %s\n", __func__);                              \
  return;

#define SKIP()                                                                 \
  tests_skipped++;                                                             \
  printf("\e[1;33m 🛇 SKIP\e[0m %s\n", __func__);                               \
  return;

#define FAIL(message, ...)                                                     \
  tests_failed++;                                                              \
  printf("\e[1;91m  ✗ FAIL\e[0m %s\n", __func__);                              \
  sprintf(fail_message, message __VA_OPT__(,) __VA_ARGS__);                    \
  printf("      %s\n\n", fail_message);                                        \
  return;

#define ASSERT_INT(expected, actual)                                            \
  if (actual != expected) {                                                     \
    FAIL("Expected integer: %d\n                   got: %d\n               at line: %d\n", expected, actual, __LINE__); \
  }

void test_vector_suite();
void test_lang_suite();

#endif
