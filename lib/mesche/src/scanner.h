#ifndef mesche_scanner_h
#define mesche_scanner_h

typedef enum {
  TokenKindNone,
  TokenKindLeftParen,
  TokenKindRightParen,
  TokenKindQuote,
  TokenKindNil,
  TokenKindTrue,
  TokenKindString,
  TokenKindSymbol,
  TokenKindKeyword,
  TokenKindNumber,
  TokenKindPlus,
  TokenKindMinus,
  TokenKindStar,
  TokenKindSlash,
  TokenKindAnd,
  TokenKindOr,
  TokenKindNot,
  TokenKindEqv,
  TokenKindEqual,
  TokenKindDefine,
  TokenKindSet,
  TokenKindBegin,
  TokenKindLet,
  TokenKindDisplay,
  TokenKindError,
  TokenKindEOF
} TokenKind;

typedef struct {
  const char *start;
  const char *current;
  int line;
} Scanner;

typedef struct {
  TokenKind kind;
  const char *start;
  int length;
  int line;
} Token;

void mesche_scanner_init(Scanner *scanner, const char *source);
Token mesche_scanner_next_token(Scanner *scanner);

#endif
