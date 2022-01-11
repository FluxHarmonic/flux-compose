#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "test.h"
#include "../lib/flux-internal.h"

#define ASSERT_KIND(_token, expected_kind)                                       \
  if (_token->kind != expected_kind) {                                           \
    FAIL("Expected kind %d, got %s", #expected_kind, _token->kind);              \
  }

#define ASSERT_STR(expected, actual)                                             \
  if (strcmp(expected, actual) != 0) {                                           \
   FAIL("Expected string: %s\n                  got: %s", expected, actual);    \
  }

#define ASSERT_INT(_token, expected)                                             \
  if (((TokenInteger*)_token)->number != expected ) {                            \
    FAIL("Expected number: %d\n                  got: %d", expected, ((TokenInteger *)_token)->number); \
  }

#define ASSERT_EQ(expected, actual)                                       \
  if (actual != expected) {                                           \
    FAIL("Expected value %d, got %s", expected, actual);              \
  }

/* #define ASSERT_FLOAT(_token, expected)                                           \ */
/*   if (_token->number != expected ) {                                             \ */
/*     FAIL("Expected number: %f\n                  got: %f", expected, _token->number);    \ */
/*   } */

TokenHeader *tokenize(char *input, TokenCursor *cursor) {
  FILE *stream = flux_file_from_string(input);
  TokenHeader *token = flux_script_tokenize(stream);
  fclose(stream);

  // Set up the cursor
  cursor->current = token;

  return token;
}

ExprHeader *parse(char *input) {
  FILE *stream = flux_file_from_string(input);
  TokenHeader *token = flux_script_tokenize(stream);
  fclose(stream);

  return flux_script_parse(token);
}

void test_lang_tokenize_empty() {
  TokenCursor token_cursor;
  TokenHeader *token = tokenize("   ", &token_cursor);

  ASSERT_KIND(token, TokenKindNone);

  PASS();
}

void test_lang_tokenize_parens() {
  TokenCursor token_cursor;
  TokenHeader *token = tokenize("()", &token_cursor);

  ASSERT_KIND(token, TokenKindParen);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindParen);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindNone);

  PASS();
}

void test_lang_tokenize_strings() {
  TokenCursor token_cursor;
  TokenHeader *token = tokenize("\"Hello world!\"", &token_cursor);

  ASSERT_KIND(token, TokenKindString);
  ASSERT_STR("Hello world!", ((TokenString*)token)->string);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindNone);

  PASS();
}

void test_lang_tokenize_symbols() {
  TokenCursor token_cursor;
  TokenHeader *token = tokenize("hello-world 'hello_world h3ll0w0rld", &token_cursor);

  ASSERT_KIND(token, TokenKindSymbol);
  ASSERT_STR("hello-world", ((TokenSymbol*)token)->string);
  ASSERT_EQ(0, ((TokenSymbol*)token)->is_quoted);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindSymbol);
  ASSERT_STR("hello_world", ((TokenSymbol*)token)->string);
  ASSERT_EQ(1, ((TokenSymbol*)token)->is_quoted);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindSymbol);
  ASSERT_STR("h3ll0w0rld", ((TokenSymbol*)token)->string);
  ASSERT_EQ(0, ((TokenSymbol*)token)->is_quoted);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindNone);

  PASS();
}

void test_lang_tokenize_keywords() {
  TokenCursor token_cursor;
  TokenHeader *token = tokenize(":hello-world :hello_world :h3ll0w0rld", &token_cursor);

  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("hello-world", ((TokenKeyword*)token)->string);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("hello_world", ((TokenKeyword*)token)->string);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("h3ll0w0rld", ((TokenKeyword*)token)->string);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindNone);

  PASS();
}

void test_lang_tokenize_numbers() {
  TokenCursor token_cursor;
  TokenHeader *token = tokenize("1 -20 300", &token_cursor);

  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT(token, 1);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT(token, -20);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT(token, 300);

  PASS();
}

void test_lang_tokenize_expressions() {
  TokenCursor token_cursor;
  TokenHeader *token = tokenize("(circle :name 'circle1 :x 200 :y -15)", &token_cursor);

  ASSERT_KIND(token, TokenKindParen);
  ASSERT_EQ(1, ((TokenParen*)token)->is_open);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindSymbol);
  ASSERT_STR("circle", ((TokenSymbol*)token)->string);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("name", ((TokenKeyword*)token)->string);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindSymbol);
  ASSERT_STR("circle1", ((TokenSymbol*)token)->string);
  ASSERT_EQ(1, ((TokenSymbol*)token)->is_quoted);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("x", ((TokenKeyword*)token)->string);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT(token, 200);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("y", ((TokenKeyword*)token)->string);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT(token, -15);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindParen);
  /* ASSERT_EQ(0, ((TokenParen*)token)->is_open); */

  /* token = flux_script_token_next(&token_cursor); */
  /* ASSERT_KIND(token, TokenKindNone); */

  PASS();
}

void test_lang_suite() {
  SUITE();

  test_lang_tokenize_empty();
  test_lang_tokenize_parens();
  test_lang_tokenize_strings();
  test_lang_tokenize_symbols();
  test_lang_tokenize_keywords();
  test_lang_tokenize_numbers();
  test_lang_tokenize_expressions();
}
