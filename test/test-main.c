#include "test.h"
#include <stdio.h>

unsigned int tests_passed = 0;
unsigned int tests_skipped = 0;
unsigned int tests_failed = 0;

char fail_message[2048];

int main(void) {
  printf("\n\e[1;36mFlux Compose Test Runner\e[0m\n");

  test_vector_suite();
  test_lang_suite();

  // Print the test report
  printf("\nTest run complete.\n\n");
  if (tests_passed > 0) {
    printf("\e[1;92m%d passed\e[0m\n", tests_passed);
  }
  if (tests_skipped > 0) {
    printf("\e[1;33m%d skipped\e[0m\n", tests_skipped);
  }
  if (tests_failed > 0) {
    printf("\e[1;91m%d failed\e[0m\n", tests_failed);
  }

  // Final newline for clarity
  printf("\n");

  return tests_failed;
}
