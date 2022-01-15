#ifndef __FLUX_INTERNAL_H
#define __FLUX_INTERNAL_H

#include <stdio.h>
#include "flux.h"

// Scripting ----------------------------------------

typedef enum {
  TokenKindNone,
  TokenKindParen,
  TokenKindString,
  TokenKindSymbol,
  TokenKindKeyword,
  TokenKindInteger,
  TokenKindFloat
} TokenKind;

typedef struct {
  unsigned int line;
  unsigned int column;
} SourceLocation;

typedef struct {
  SourceLocation location;
  TokenKind kind;
} TokenHeader;

typedef struct {
  TokenHeader header;
  char is_open;
} TokenParen;

typedef struct {
  TokenHeader header;
  unsigned int length;
  char string[];
} TokenString;

typedef struct {
  TokenHeader header;
  unsigned int length;
  char is_quoted;
  char string[];
} TokenSymbol;

typedef TokenString TokenKeyword;

typedef struct {
  TokenHeader header;
  int number;
} TokenInteger;

typedef struct {
  TokenHeader header;
  float number;
} TokenFloat;

typedef struct {
  TokenHeader *current;
} TokenCursor;

typedef enum {
  ValueKindNone,
  ValueKindInteger,
  ValueKindFloat,
  ValueKindString,
  ValueKindEntity,
} ValueKind;

typedef struct {
  ValueKind kind;
} ValueHeader;

typedef struct {
  ValueHeader header;
  int value;
} ValueInteger;

typedef struct {
  ValueHeader header;
  double value;
} ValueFloat;

typedef struct {
  ValueHeader header;
  unsigned int length;
  char string[];
} ValueString;

typedef struct {
  ValueHeader header;
  void* entity;
} ValueEntity;

typedef struct {
  ValueHeader *current;
} ValueCursor;

typedef struct {
  ValueHeader header;
} ValueFunctionPtr;

typedef enum {
  ExprKindNone,
  ExprKindList,
  ExprKindSymbol,
  ExprKindKeyword,
  ExprKindValue
} ExprKind;

typedef struct {
  ExprKind kind;
} ExprHeader;

typedef struct {
  ExprHeader header;
  char is_quoted;
  unsigned int length;
  char name[];
} ExprSymbol;

typedef struct {
  ExprHeader header;
  unsigned int length;
  char name[];
} ExprKeyword;

typedef struct {
  ExprHeader header;

  // Do not trust the size of this field! Use the function
  // flux_script_value_size to get the full size of ExprValue.
  ValueHeader value;
} ExprValue;

typedef struct {
  ExprHeader header;
  unsigned int length;
  ExprHeader items[];
} ExprList;

typedef struct {
  ExprList *list;
  unsigned int index;
  ExprHeader *current;
} ExprListCursor;

extern TokenHeader *flux_script_tokenize(FILE *script_file);
extern TokenHeader *flux_script_token_next(TokenCursor *token_cursor);
extern ExprHeader *flux_script_expr_list_next(ExprListCursor *iterator);
extern ExprList *flux_script_parse(TokenHeader *start_token);
extern void *flux_script_expr_list_cursor_init(ExprList *list, ExprListCursor *list_cursor);
extern ValueHeader *flux_script_eval_expr(ExprHeader *expr, ValueCursor *value_cursor);

#endif
