#ifndef __FLUX_INTERNAL_H
#define __FLUX_INTERNAL_H

#include <flux.h>
#include <stdio.h>

// Vector -------------------------------------------

struct _Vector {
  uint32_t length;
  void *start_item;
  size_t buffer_size;
  size_t buffer_usage;
  VectorItemSizeFunc item_size_func;
};

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
  char string[128];
} TokenString;

typedef struct {
  TokenHeader header;
  unsigned int length;
  char is_quoted;
  char string[128];
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

typedef struct {
  ValueHeader *current;
} ValueCursor;

typedef enum {
  ExprKindNone,
  ExprKindList,
  ExprKindSymbol,
  ExprKindKeyword,
  ExprKindInteger,
  ExprKindString
} ExprKind;

typedef struct {
  ExprKind kind;
} ExprHeader;

typedef struct {
  ExprHeader header;
  char is_quoted;
  unsigned int length;
  char name[32];
} ExprSymbol;

typedef struct {
  ExprHeader header;
  unsigned int length;
  char name[32];
} ExprKeyword;

typedef struct {
  ExprHeader header;
  int number;
} ExprInteger;

typedef struct {
  ExprHeader header;
  unsigned int length;
  char string[128];
} ExprString;

typedef struct {
  ExprHeader header;
  struct _Vector items;
} ExprList;

extern Vector flux_script_tokenize(FILE *script_file);
extern Vector flux_script_parse(Vector token_vector);
extern ValueHeader *flux_script_eval_expr(ExprHeader *expr);

// Graphics -------------------------------------------

extern ValueHeader *flux_graphics_func_show_preview_window(VectorCursor *list_cursor,
                                                           ValueCursor *value_cursor);
extern ValueHeader *flux_graphics_func_circle(VectorCursor *list_cursor, ValueCursor *value_cursor);

#endif
