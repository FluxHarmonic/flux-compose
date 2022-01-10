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
  long number;
} TokenInteger;

typedef struct {
  TokenHeader header;
  double number;
} TokenFloat;

extern TokenHeader *flux_script_tokenize(FILE *script_file);
extern TokenHeader *flux_script_token_next(TokenHeader *token);

#endif
