#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "op.h"
#include "util.h"
#include "chunk.h"
#include "disasm.h"
#include "object.h"
#include "scanner.h"
#include "compiler.h"

// NOTE: Enable this for diagnostic purposes
#define DEBUG_PRINT_CODE

typedef struct {
  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;
} Parser;

typedef struct {
  VM *vm;
  Parser *parser;
  Scanner *scanner;
  Chunk *chunk;
} CompilerContext;

static void compiler_emit_byte(CompilerContext *ctx, uint8_t byte) {
  mesche_chunk_write(ctx->chunk, byte, ctx->parser->previous.line);
}

static void compiler_emit_bytes(CompilerContext *ctx, uint8_t byte1, uint8_t byte2) {
  compiler_emit_byte(ctx, byte1);
  compiler_emit_byte(ctx, byte2);
}

static void compiler_emit_return(CompilerContext *ctx) {
  compiler_emit_byte(ctx, OP_RETURN);
}

static uint8_t compiler_make_constant(CompilerContext *ctx, Value value) {
  int constant = mesche_chunk_constant_add(ctx->chunk, value);
  // TODO: We'll want to make sure we have at least 16 bits for constants
  if (constant > UINT8_MAX) {
    PANIC("Too many constants in one chunk!\n");
  }

  return (uint8_t)constant;
}

static void compiler_emit_constant(CompilerContext *ctx, Value value) {
  compiler_emit_bytes(ctx, OP_CONSTANT, compiler_make_constant(ctx, value));
}

static void compiler_error_at_token(Parser *parser, Token *token, const char *message) {
  // If we're already in panic mode, ignore errors to avoid spam
  if (parser->panic_mode) return;

  // Turn on panic mode until we resynchronize
  parser->panic_mode = true;

  printf("[line %d] Error", token->line);
  if (token->kind == TokenKindEOF) {
    printf(" at end");
  } else if (token->kind == TokenKindError) {
    // Already an error
  } else {
    printf(" at '%.*s'", token->length, token->start);
  }

  printf(":  %s\n", message);

  parser->had_error = true;
}

void compiler_error(Parser *parser, const char *message) {
  compiler_error_at_token(parser, &parser->previous, message);
}

void compiler_error_at_current(Parser *parser, const char *message) {
  compiler_error_at_token(parser, &parser->current, message);
}

void compiler_advance(CompilerContext *ctx) {
  ctx->parser->previous = ctx->parser->current;

  for (;;) {
    ctx->parser->current = mesche_scanner_next_token(ctx->scanner);
    // Consume tokens until we hit a non-error token
    if (ctx->parser->current.kind != TokenKindError) break;

    // Create a parse error
    // TODO: Correct error
    compiler_error_at_current(ctx->parser, "Hup");
  }

  /* printf("ADVANCED TO: %d\n", ctx->parser->current.kind); */
}

void compiler_consume(CompilerContext *ctx, TokenKind kind, const char *message) {
  if (ctx->parser->current.kind == kind) {
    /* printf("CONSUME %d\n", kind); */
    compiler_advance(ctx);
    return;
  }

  // If we didn't reach the expected token time, return an error
  compiler_error_at_current(ctx->parser, message);
}

void compiler_parse_operator_call(CompilerContext *ctx, Token *call_token) {
  TokenKind operator = call_token->kind;
  switch(operator) {
  case TokenKindPlus: compiler_emit_byte(ctx, OP_ADD); break;
  case TokenKindMinus: compiler_emit_byte(ctx, OP_SUBTRACT); break;
  case TokenKindStar: compiler_emit_byte(ctx, OP_MULTIPLY); break;
  case TokenKindSlash: compiler_emit_byte(ctx, OP_DIVIDE); break;
  case TokenKindAnd:  compiler_emit_byte(ctx, OP_AND); break;
  case TokenKindOr:  compiler_emit_byte(ctx, OP_OR); break;
  case TokenKindNot:  compiler_emit_byte(ctx, OP_NOT); break;
  case TokenKindEqv:  compiler_emit_byte(ctx, OP_EQV); break;
  case TokenKindEqual:  compiler_emit_byte(ctx, OP_EQUAL); break;
  default: return; // We shouldn't hit this
  }
}

// Predefine the main parser function
void compiler_parse_expr(CompilerContext *ctx);

void compiler_parse_list(CompilerContext *ctx) {
  // Try to find the call target (this could be an expression!)
  Token call_token = ctx->parser->current;
  compiler_advance(ctx);

  if (ctx->parser->current.kind == TokenKindRightParen) {
    // Is it an empty list?  Error if not quoted
    PANIC("Empty list!");
  }

  // Parse expressions until we reach a right paren
  for (;;) {
    // Compile next operand
    compiler_parse_expr(ctx);

    // Bail out when we hit the closing parentheses
    if (ctx->parser->current.kind == TokenKindRightParen) {
      // Compile the call operator
      compiler_parse_operator_call(ctx, &call_token);

      // Consume the right paren and exit
      compiler_consume(ctx, TokenKindRightParen, "Expected closing paren.");
      return;
    }
  }
}

void (*ParserFunc)(CompilerContext *ctx);

void compiler_parse_number(CompilerContext *ctx) {
  double value = strtod(ctx->parser->previous.start, NULL);
  compiler_emit_constant(ctx, NUMBER_VAL(value));
}

void compiler_parse_string(CompilerContext *ctx) {
  compiler_emit_constant(ctx,
                         OBJECT_VAL(mesche_object_make_string(ctx->vm,
                                                              ctx->parser->previous.start + 1,
                                                              ctx->parser->previous.length - 2)));
}

void compiler_parse_literal(CompilerContext *ctx) {
  switch (ctx->parser->previous.kind) {
  case TokenKindNil: compiler_emit_byte(ctx, OP_NIL); break;
  case TokenKindTrue: compiler_emit_byte(ctx, OP_T); break;
  default: return; // We shouldn't hit this
  }
}

void compiler_parse_expr(CompilerContext *ctx) {
  /* static const ParserFunc[TokenKindEOF] = [NULL, */
  /*                                          NULL, // TokenKindParen */
  /*                                          compiler_parse_list, // TokenKindLeftParen, */
  /*                                          NULL,              // TokenKindRightParen, */
  /* ]; */

  if (ctx->parser->current.kind == TokenKindEOF) {
    PANIC("Reached EOF token!\n");
  }

  // An expression can either be a single element or a list
  if (ctx->parser->current.kind == TokenKindLeftParen) {
    compiler_advance(ctx);
    compiler_parse_list(ctx);
    return;
  }

  // Is it a list that starts with a quote?

  // Must be an atom
  if (ctx->parser->current.kind == TokenKindNumber) {
    compiler_advance(ctx);
    compiler_parse_number(ctx);
  } else if (ctx->parser->current.kind == TokenKindNil) {
    compiler_advance(ctx);
    compiler_parse_literal(ctx);
  } else if (ctx->parser->current.kind == TokenKindTrue) {
    compiler_advance(ctx);
    compiler_parse_literal(ctx);
  } else if (ctx->parser->current.kind == TokenKindString) {
    compiler_advance(ctx);
    compiler_parse_string(ctx);
  }
}

void compiler_end(CompilerContext *ctx) {
  compiler_emit_return(ctx);

  #ifdef DEBUG_PRINT_CODE
  if (!ctx->parser->had_error) {
    mesche_disasm_chunk(ctx->chunk, "code");
  }
  #endif
}

bool mesche_compile_source(VM *vm, const char *script_source, Chunk *chunk) {
  Parser parser;
  Scanner scanner;
  mesche_scanner_init(&scanner, script_source);

  // Set up the context
  CompilerContext ctx = {
    .vm = vm,
    .parser = &parser,
    .scanner = &scanner,
    .chunk = chunk
  };

  parser.had_error = false;
  parser.panic_mode = false;

  // Find the first legitimate token and then start parsing
  compiler_advance(&ctx);
  compiler_parse_expr(&ctx);
  compiler_consume(&ctx, TokenKindEOF, "Expected end of expression.");

  compiler_end(&ctx);

  return !parser.had_error;
}
