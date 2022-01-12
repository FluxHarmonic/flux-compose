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

#define ASSERT_INT_TOKEN(_token, expected)                                             \
  if (((TokenInteger*)_token)->number != expected ) {                            \
    FAIL("Expected number: %d\n                  got: %d", expected, ((TokenInteger *)_token)->number); \
  }

#define ASSERT_INT_VALUE(_value, expected)                                             \
  if (((ValueInteger*)&_value)->value != expected ) {                            \
    FAIL("Expected integer: %d\n                   got: %d", expected, ((ValueInteger*)&_value)->value); \
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

ExprList *parse(char *input, ExprListCursor *list_cursor) {
  FILE *stream = flux_file_from_string(input);
  TokenHeader *token = flux_script_tokenize(stream);
  fclose(stream);

  // Set up the cursor
  list_cursor->list = flux_script_parse(token);
  list_cursor->current = NULL;
  list_cursor->index = 0;

  return list_cursor->list;
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
  ASSERT_INT_TOKEN(token, 1);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT_TOKEN(token, -20);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT_TOKEN(token, 300);

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
  ASSERT_INT_TOKEN(token, 200);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("y", ((TokenKeyword*)token)->string);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT_TOKEN(token, -15);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindParen);
  ASSERT_EQ(0, ((TokenParen*)token)->is_open);

  token = flux_script_token_next(&token_cursor);
  ASSERT_KIND(token, TokenKindNone);

  PASS();
}

void test_lang_parse_list() {
  ExprHeader *expr = NULL;
  ExprListCursor list_cursor;
  parse("(circle :name \"circle1\" :x 200 :y -15)", &list_cursor);

  printf("--- START CHECKS\n");

  // Every result is wrapped in a top-level list
  expr = flux_script_expr_list_next(&list_cursor);
  ASSERT_EQ(ExprKindList, expr->kind);

  // Prepare a cursor for the sub list
  ExprListCursor sub_list_cursor;
  flux_script_expr_list_cursor_init(expr, &sub_list_cursor);

  // circle
  expr = flux_script_expr_list_next(&sub_list_cursor);
  flux_log_mem(expr, "Check for symbol!\n");
  ASSERT_EQ(ExprKindSymbol, expr->kind);
  ASSERT_STR("circle", ((ExprSymbol *)expr)->name);

  // :name
  expr = flux_script_expr_list_next(&sub_list_cursor);
  flux_log_mem(expr, "Check for keyword!\n");
  ASSERT_EQ(ExprKindKeyword, expr->kind);
  ASSERT_STR("name", ((ExprKeyword *)expr)->name);

  // "circle1"
  expr = flux_script_expr_list_next(&sub_list_cursor);
  flux_log_mem(expr, "Check for string!\n");
  ASSERT_EQ(ExprKindValue, ((ExprValue *)expr)->header.kind);
  ASSERT_EQ(ValueKindString, ((ExprValue *)expr)->value.kind);
  ASSERT_STR("circle1", ((ValueString*)&((ExprValue *)expr)->value)->string);

  // :x
  expr = flux_script_expr_list_next(&sub_list_cursor);
  flux_log_mem(expr, "Check for keyword!\n");
  ASSERT_EQ(ExprKindKeyword, expr->kind);
  ASSERT_STR("x", ((ExprKeyword *)expr)->name);

  // 200
  expr = flux_script_expr_list_next(&sub_list_cursor);
  flux_log_mem(expr, "Check for integer!\n");
  ASSERT_EQ(ExprKindValue, ((ExprValue *)expr)->header.kind);
  ASSERT_EQ(ValueKindInteger, ((ExprValue *)expr)->value.kind);
  ASSERT_INT_VALUE(((ExprValue *)expr)->value, 200);

  // :y
  expr = flux_script_expr_list_next(&sub_list_cursor);
  flux_log_mem(expr, "Check for keyword!\n");
  ASSERT_EQ(ExprKindKeyword, expr->kind);
  ASSERT_STR("y", ((ExprKeyword *)expr)->name);

  // -15
  expr = flux_script_expr_list_next(&sub_list_cursor);
  flux_log_mem(expr, "Check for integer!\n");
  ASSERT_EQ(ExprKindValue, expr->kind);
  ASSERT_EQ(ValueKindInteger, ((ExprValue *)expr)->value.kind);
  ASSERT_INT_VALUE(((ExprValue *)expr)->value, -15);

  expr = flux_script_expr_list_next(&sub_list_cursor);
  flux_log_mem(expr, "Check end of sequence!\n");
  ASSERT_EQ(ExprKindNone, expr->kind);

  printf("After check\n");
}

void test_lang_parse_multiple_exprs() {
  FAIL("Not implemented yet.");
}

void test_lang_parse_nested_lists() {
  FAIL("Not implemented yet.");
}

void test_lang_suite() {
  SUITE();

  // TODO: Need tests for error cases!

  test_lang_tokenize_empty();
  test_lang_tokenize_parens();
  test_lang_tokenize_strings();
  test_lang_tokenize_symbols();
  test_lang_tokenize_keywords();
  test_lang_tokenize_numbers();
  test_lang_tokenize_expressions();

  test_lang_parse_list();
  test_lang_parse_multiple_exprs();
  test_lang_parse_nested_lists();
}
