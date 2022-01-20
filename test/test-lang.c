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
    FAIL("Expected value %d, got %d", expected, actual);              \
  }

/* #define ASSERT_FLOAT(_token, expected)                                           \ */
/*   if (_token->number != expected ) {                                             \ */
/*     FAIL("Expected number: %f\n                  got: %f", expected, _token->number);    \ */
/*   } */

TokenHeader *tokenize(char *input, VectorCursor *cursor) {
  FILE *stream = flux_file_from_string(input);
  Vector token_vector = flux_script_tokenize(stream);
  fclose(stream);

  // Set up the cursor
  flux_vector_cursor_init(token_vector, cursor);

  return flux_vector_cursor_next(cursor);
}

Vector parse(char *input, VectorCursor *list_cursor) {
  FILE *stream = flux_file_from_string(input);
  Vector token_vector = flux_script_tokenize(stream);
  fclose(stream);

  // Set up the cursor
  Vector expr_vector = flux_script_parse(token_vector);
  flux_vector_cursor_init(expr_vector, list_cursor);

  return expr_vector;
}

ValueHeader *eval(char *input) {
  FILE *stream = flux_file_from_string(input);
  Vector token_vector = flux_script_tokenize(stream);
  fclose(stream);

  // Eval the resulting expression and return it
  ValueCursor value_cursor;
  Vector result = flux_script_parse(token_vector);
  return flux_script_eval_expr((ExprHeader *)&result->start_item);
}

void test_lang_tokenize_empty() {
  VectorCursor token_cursor;
  TokenHeader *token = tokenize("   ", &token_cursor);

  ASSERT_KIND(token, TokenKindNone);

  PASS();
}

void test_lang_tokenize_parens() {
  VectorCursor token_cursor;
  TokenHeader *token = tokenize("()", &token_cursor);

  ASSERT_KIND(token, TokenKindParen);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindParen);

  ASSERT_INT(0, flux_vector_cursor_has_next(&token_cursor));

  PASS();
}

void test_lang_tokenize_strings() {
  VectorCursor token_cursor;
  TokenHeader *token = tokenize("\"Hello world!\"", &token_cursor);

  ASSERT_KIND(token, TokenKindString);
  ASSERT_STR("Hello world!", ((TokenString *)token)->string);

  ASSERT_INT(0, flux_vector_cursor_has_next(&token_cursor));

  PASS();
}

void test_lang_tokenize_symbols() {
  VectorCursor token_cursor;
  TokenHeader *token = tokenize("hello-world 'hello_world h3ll0w0rld", &token_cursor);

  ASSERT_KIND(token, TokenKindSymbol);
  ASSERT_STR("hello-world", ((TokenSymbol *)token)->string);
  ASSERT_EQ(0, ((TokenSymbol *)token)->is_quoted);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindSymbol);
  ASSERT_STR("hello_world", ((TokenSymbol *)token)->string);
  ASSERT_EQ(1, ((TokenSymbol *)token)->is_quoted);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindSymbol);
  ASSERT_STR("h3ll0w0rld", ((TokenSymbol *)token)->string);
  ASSERT_EQ(0, ((TokenSymbol *)token)->is_quoted);

  ASSERT_INT(0, flux_vector_cursor_has_next(&token_cursor));

  PASS();
}

void test_lang_tokenize_keywords() {
  VectorCursor token_cursor;
  TokenHeader *token = tokenize(":hello-world :hello_world :h3ll0w0rld", &token_cursor);

  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("hello-world", ((TokenKeyword *)token)->string);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("hello_world", ((TokenKeyword *)token)->string);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("h3ll0w0rld", ((TokenKeyword *)token)->string);

  ASSERT_INT(0, flux_vector_cursor_has_next(&token_cursor));

  PASS();
}

void test_lang_tokenize_numbers() {
  VectorCursor token_cursor;
  TokenHeader *token = tokenize("1 -20 300", &token_cursor);

  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT_TOKEN(token, 1);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT_TOKEN(token, -20);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT_TOKEN(token, 300);

  ASSERT_INT(0, flux_vector_cursor_has_next(&token_cursor));

  PASS();
}

void test_lang_tokenize_expressions() {
  VectorCursor token_cursor;
  TokenHeader *token =
      tokenize("(circle :name 'circle1 :x 200 :y -15)", &token_cursor);

  ASSERT_KIND(token, TokenKindParen);
  ASSERT_EQ(1, ((TokenParen *)token)->is_open);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindSymbol);
  ASSERT_STR("circle", ((TokenSymbol *)token)->string);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("name", ((TokenKeyword *)token)->string);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindSymbol);
  ASSERT_STR("circle1", ((TokenSymbol *)token)->string);
  ASSERT_EQ(1, ((TokenSymbol *)token)->is_quoted);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("x", ((TokenKeyword *)token)->string);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT_TOKEN(token, 200);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindKeyword);
  ASSERT_STR("y", ((TokenKeyword *)token)->string);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindInteger);
  ASSERT_INT_TOKEN(token, -15);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindParen);
  ASSERT_EQ(0, ((TokenParen *)token)->is_open);

  ASSERT_INT(0, flux_vector_cursor_has_next(&token_cursor));

  PASS();
}

void test_lang_tokenize_single_symbol_list() {
  VectorCursor token_cursor;
  TokenHeader *token = tokenize("(circle)", &token_cursor);

  ASSERT_KIND(token, TokenKindParen);
  ASSERT_EQ(1, ((TokenParen *)token)->is_open);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindSymbol);
  ASSERT_STR("circle", ((TokenSymbol *)token)->string);

  token = flux_vector_cursor_next(&token_cursor);
  ASSERT_KIND(token, TokenKindParen);
  ASSERT_EQ(0, ((TokenParen *)token)->is_open);

  ASSERT_INT(0, flux_vector_cursor_has_next(&token_cursor));

  PASS();
}

void test_lang_parse_list() {
  ExprHeader *expr = NULL;
  VectorCursor list_cursor;
  parse("(circle :name \"circle1\" :x 200 :y -15)", &list_cursor);

  // Every result is wrapped in a top-level list
  expr = flux_vector_cursor_next(&list_cursor);
  ASSERT_EQ(ExprKindList, expr->kind);

  // Prepare a cursor for the sub list
  VectorCursor sub_list_cursor;
  flux_vector_cursor_init(&((ExprList*)expr)->items, &sub_list_cursor);

  // circle
  expr = flux_vector_cursor_next(&sub_list_cursor);
  ASSERT_EQ(ExprKindSymbol, expr->kind);
  ASSERT_STR("circle", ((ExprSymbol *)expr)->name);

  // :name
  expr = flux_vector_cursor_next(&sub_list_cursor);
  ASSERT_EQ(ExprKindKeyword, expr->kind);
  ASSERT_STR("name", ((ExprKeyword *)expr)->name);

  // "circle1"
  expr = flux_vector_cursor_next(&sub_list_cursor);
  ASSERT_EQ(ExprKindString, ((ExprString *)expr)->header.kind);
  ASSERT_STR("circle1", ((ExprString *)expr)->string);

  // :x
  expr = flux_vector_cursor_next(&sub_list_cursor);
  ASSERT_EQ(ExprKindKeyword, expr->kind);
  ASSERT_STR("x", ((ExprKeyword *)expr)->name);

  // 200
  expr = flux_vector_cursor_next(&sub_list_cursor);
  ASSERT_EQ(ExprKindInteger, ((ExprInteger *)expr)->header.kind);
  ASSERT_EQ(200, ((ExprInteger *)expr)->number);

  // :y
  expr = flux_vector_cursor_next(&sub_list_cursor);
  ASSERT_EQ(ExprKindKeyword, expr->kind);
  ASSERT_STR("y", ((ExprKeyword *)expr)->name);

  // -15
  expr = flux_vector_cursor_next(&sub_list_cursor);
  ASSERT_EQ(ExprKindInteger, expr->kind);
  ASSERT_EQ(-15, ((ExprInteger*)expr)->number);

  ASSERT_EQ(0, flux_vector_cursor_has_next(&sub_list_cursor));
  ASSERT_EQ(0, flux_vector_cursor_has_next(&list_cursor));

  PASS();
}

void test_lang_parse_multiple_exprs() {
  ExprHeader *expr = NULL;
  VectorCursor list_cursor;
  parse("(circle 200) (do-something)", &list_cursor);

  /* FAIL("Not implemented yet."); */
}

void test_lang_parse_nested_lists() {
  ExprHeader *expr = NULL;
  VectorCursor list_cursor;
  parse("(circle :color (rgb 255 0 0))", &list_cursor);

  /* FAIL("Not implemented yet."); */
}

void test_lang_eval_integer() {
  SKIP();

  ValueHeader *value = eval("311");

  ASSERT_EQ(ValueKindInteger, value->kind);
  // TODO: I shouldn't have to dereference this, rethink the macro
  ASSERT_INT_VALUE(*value, 311);

  PASS();
}

void test_lang_eval_string() {
  SKIP();

  ValueHeader *value = eval("\"Flux Harmonic\"");

  ASSERT_EQ(ValueKindString, value->kind);
  ASSERT_STR("Flux Harmonic", ((ValueString *)value)->string);

  PASS();
}

void test_lang_eval_basic_call() {
  SKIP();

  ValueHeader *value = eval("(add 1 2)");

  ASSERT_EQ(ValueKindInteger, value->kind);
  ASSERT_INT_VALUE(*value, 3);

  PASS();
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
  test_lang_tokenize_single_symbol_list();

  test_lang_parse_list();
  test_lang_parse_multiple_exprs();
  test_lang_parse_nested_lists();

  test_lang_eval_integer();
  test_lang_eval_string();
  test_lang_eval_basic_call();
}
