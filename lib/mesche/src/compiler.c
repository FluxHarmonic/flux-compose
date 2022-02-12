#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "op.h"
#include "util.h"
#include "chunk.h"
#include "disasm.h"
#include "object.h"
#include "scanner.h"
#include "compiler.h"

#define UINT8_COUNT (UINT8_MAX + 1)

// NOTE: Enable this for diagnostic purposes
#define DEBUG_PRINT_CODE

typedef struct {
  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;
} Parser;

typedef struct {
  Token name;
  int depth;
} Local;

typedef struct {
  VM *vm;
  Parser *parser;
  Scanner *scanner;
  Chunk *chunk;

  Local locals[UINT8_COUNT];
  int local_count;
  int scope_depth;
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

static void compiler_error_at_token(CompilerContext *ctx, Token *token, const char *message) {
  // If we're already in panic mode, ignore errors to avoid spam
  if (ctx->parser->panic_mode) return;

  // Turn on panic mode until we resynchronize
  ctx->parser->panic_mode = true;

  printf("[line %d] Error", token->line);
  if (token->kind == TokenKindEOF) {
    printf(" at end");
  } else if (token->kind == TokenKindError) {
    // Already an error
  } else {
    printf(" at '%.*s'", token->length, token->start);
  }

  printf(":  %s\n", message);

  ctx->parser->had_error = true;
}

static void compiler_error(CompilerContext *ctx, const char *message) {
  compiler_error_at_token(ctx, &ctx->parser->previous, message);
}

static void compiler_error_at_current(CompilerContext *ctx, const char *message) {
  compiler_error_at_token(ctx, &ctx->parser->current, message);
}

static void compiler_advance(CompilerContext *ctx) {
  ctx->parser->previous = ctx->parser->current;

  for (;;) {
    ctx->parser->current = mesche_scanner_next_token(ctx->scanner);
    // Consume tokens until we hit a non-error token
    if (ctx->parser->current.kind != TokenKindError) break;

    // Create a parse error
    // TODO: Correct error
    compiler_error_at_current(ctx, "Reached a compiler error");
  }
}

static void compiler_consume(CompilerContext *ctx, TokenKind kind, const char *message) {
  if (ctx->parser->current.kind == kind) {
    compiler_advance(ctx);
    return;
  }

  // If we didn't reach the expected token time, return an error
  compiler_error_at_current(ctx, message);
}

// Predefine the main parser function
static void compiler_parse_expr(CompilerContext *ctx);

static void compiler_parse_block(CompilerContext *ctx, bool expect_end_paren) {
  // TODO: Discard every value except for the last!
  for (;;) {
    compiler_parse_expr(ctx);

    if (expect_end_paren && ctx->parser->current.kind == TokenKindRightParen) {
      compiler_consume(ctx, TokenKindRightParen, "Expected closing paren.");
      break;
    } else if (ctx->parser->current.kind == TokenKindEOF) {
      break;
    } else {
      // If we continue the loop, pop the last expression result
      compiler_emit_byte(ctx, OP_POP);
    }
  }
}

static bool compiler_identifiers_equal(Token *a, Token *b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static void compiler_add_local(CompilerContext *ctx, Token name) {
  if (ctx->local_count == UINT8_COUNT) {
    compiler_error(ctx, "Too many local variables defined in function.");
  }
  Local *local = &ctx->locals[ctx->local_count++];
  local->name = name; // No need to copy, will only be used during compilation
  local->depth = -1; // The variable is uninitialized until assigned
}

static int compiler_resolve_local(CompilerContext *ctx, Token *name) {
  // Find the identifier by name in scope chain
  for (int i = ctx->local_count - 1; i >= 0; i--) {
    Local *local = &ctx->locals[i];
    if (compiler_identifiers_equal(name, &local->name)) {
      if (local->depth == -1) {
        compiler_error(ctx, "Referenced variable before it was bound");
      }
      return i;
    }
  }

  return -1;
}

static void compiler_local_mark_initialized(CompilerContext *ctx) {
  // Mark the latest local variable as initialized
  ctx->locals[ctx->local_count - 1].depth = ctx->scope_depth;
}

static void compiler_declare_variable(CompilerContext *ctx) {
  // No need to declare a global, it will be dynamically resolved
  if (ctx->scope_depth == 0) return;

  // Start from the most recent local and work backwards to
  // find an existing variable with the same binding in this scope
  Token *name = &ctx->parser->previous;
  for (int i = ctx->local_count - 1; i >= 0; i--) {
    // If the local is in a different scope, stop looking
    Local *local = &ctx->locals[i];
    if (local->depth != -1 && local->depth < ctx->scope_depth) break;

    // In the same scope, is the identifier the same?
    if (compiler_identifiers_equal(name, &local->name)) {
      compiler_error(ctx, "Duplicate variable binding in 'let'");
      return;
    }
  }

  // Add a local binding for the name
  compiler_add_local(ctx, *name);
}

static uint8_t compiler_parse_symbol(CompilerContext *ctx) {
  // Declare the variable and exit if we're in a local scope
  compiler_declare_variable(ctx);
  if (ctx->scope_depth > 0) return 0;

  // TODO: Find the index of an existing constant for the same symbol!
  return
    compiler_make_constant(ctx,
                           OBJECT_VAL(mesche_object_make_string(ctx->vm,
                                                                ctx->parser->previous.start,
                                                                ctx->parser->previous.length)));
}

static void compiler_parse_identifier(CompilerContext *ctx) {
  // Are we looking at a local variable?
  int local_index = compiler_resolve_local(ctx, &ctx->parser->previous);
  if (local_index != -1) {
    compiler_emit_bytes(ctx, OP_READ_LOCAL, (uint8_t)local_index);
  } else {
    uint8_t variable_constant = compiler_parse_symbol(ctx);
    compiler_emit_bytes(ctx, OP_READ_GLOBAL, variable_constant);
  }
}

static void compiler_define_variable(CompilerContext *ctx, uint8_t variable_constant) {
  // We don't define global variables in local scopes
  if (ctx->scope_depth > 0) {
    // TODO: Is this necessary?
    compiler_local_mark_initialized(ctx);
    return;
  }

  compiler_emit_bytes(ctx, OP_DEFINE_GLOBAL, variable_constant);
}

static void compiler_parse_define(CompilerContext *ctx) {
  compiler_consume(ctx, TokenKindSymbol, "Expected symbol after 'define'");

  // TODO: Only allow defines at the top of let/lambda bodies
  uint8_t variable_constant = compiler_parse_symbol(ctx);
  compiler_parse_expr(ctx);
  compiler_define_variable(ctx, variable_constant);

  compiler_consume(ctx, TokenKindRightParen, "Expected closing paren.");
}

static void compiler_parse_set(CompilerContext *ctx) {
  compiler_consume(ctx, TokenKindSymbol, "Expected symbol after 'set!'");

  uint8_t instr = OP_SET_LOCAL;
  int arg = compiler_resolve_local(ctx, &ctx->parser->previous);

  // If there isn't a local, use a global variable instead
  if (arg == -1) {
    arg = compiler_parse_symbol(ctx);
    instr = OP_SET_GLOBAL;
  }

  compiler_parse_expr(ctx);
  compiler_emit_bytes(ctx, instr, arg);

  compiler_consume(ctx, TokenKindRightParen, "Expected closing paren.");
}

static void compiler_begin_scope(CompilerContext *ctx) {
  ctx->scope_depth++;
}

static void compiler_end_scope(CompilerContext *ctx) {
  // Pop all local variables from the previous scope
  // TODO: Count the variables to pop and send it in one OP_POPN
  ctx->scope_depth--;
  while (ctx->local_count > 0 && ctx->locals[ctx->local_count - 1].depth > ctx->scope_depth) {
    compiler_emit_byte(ctx, OP_POP);
    ctx->local_count--;
  }
}

static void compiler_parse_let(CompilerContext *ctx) {
  compiler_consume(ctx, TokenKindLeftParen, "Expected left paren after 'let'");

  compiler_begin_scope(ctx);

  for (;;) {
    if (ctx->parser->current.kind == TokenKindRightParen) {
      compiler_consume(ctx, TokenKindRightParen, "Expected right paren to end bindings");
      break;
    }

    compiler_consume(ctx, TokenKindLeftParen, "Expected left paren to start binding pair");

    // compiler_parse_symbol expects the symbol token to be in parser->previous
    compiler_advance(ctx);
    compiler_parse_symbol(ctx);
    compiler_parse_expr(ctx);
    compiler_define_variable(ctx, 0 /* Irrelevant, this is local */);

    compiler_consume(ctx, TokenKindRightParen, "Expected right paren to end binding pair");
  }

  compiler_parse_block(ctx, true);
  compiler_end_scope(ctx);
}

static int compiler_emit_jump(CompilerContext *ctx, uint8_t instruction) {
  compiler_emit_byte(ctx, instruction);
  compiler_emit_byte(ctx, 0xff);
  compiler_emit_byte(ctx, 0xff);

  return ctx->chunk->count - 2;
}

static void compiler_patch_jump(CompilerContext *ctx, int offset) {
  // We offset by -2 to adjust for the size of the jump offset itself
  int jump = ctx->chunk->count - offset - 2;

  if (jump > UINT16_MAX) {
    compiler_error(ctx, "Attempting to emit jump that is larger than possible jump size.");
  }

  // Place the two bytes with the jump delta
  ctx->chunk->code[offset] = (jump >> 8) & 0xff;
  ctx->chunk->code[offset + 1] = jump & 0xff;
}

static void compiler_parse_if(CompilerContext *ctx) {
  // Parse predicate
  compiler_parse_expr(ctx);
  int jump_origin = compiler_emit_jump(ctx, OP_JUMP_IF_FALSE);

  // Include a pop so that the expression value gets removed from the stack in the truth path
  compiler_emit_byte(ctx, OP_POP);

  // Parse truth expr
  compiler_parse_expr(ctx);

  int else_jump = compiler_emit_jump(ctx, OP_JUMP);

  // Patch the jump instruction after the truth path has been compiled
  compiler_patch_jump(ctx, jump_origin);

  // Include a pop so that the expression value gets removed from the stack
  compiler_emit_byte(ctx, OP_POP);

  // Parse false expr
  compiler_parse_expr(ctx);

  // Patch the jump instruction after the false path has been compiled
  compiler_patch_jump(ctx, else_jump);

  compiler_consume(ctx, TokenKindRightParen, "Expected right paren to end 'if' expression");
}

static void compiler_parse_operator_call(CompilerContext *ctx, Token *call_token) {
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
  case TokenKindDisplay:  compiler_emit_byte(ctx, OP_DISPLAY); break;
  default: return; // We shouldn't hit this
  }
}

static bool compiler_parse_special_form(CompilerContext *ctx, Token *call_token) {
  TokenKind operator = call_token->kind;
  switch(operator) {
  case TokenKindBegin: compiler_parse_block(ctx, true); break;
  case TokenKindDefine: compiler_parse_define(ctx); break;
  case TokenKindSet: compiler_parse_set(ctx); break;
  case TokenKindLet: compiler_parse_let(ctx); break;
  case TokenKindIf: compiler_parse_if(ctx); break;
  default: return false; // No special form found
  }

  return true;
}

static void compiler_parse_list(CompilerContext *ctx) {
  // Try to find the call target (this could be an expression!)
  Token call_token = ctx->parser->current;
  compiler_advance(ctx);

  if (ctx->parser->current.kind == TokenKindRightParen) {
    // Is it an empty list?  Error if not quoted
    PANIC("Empty list!");
  }

  if (compiler_parse_special_form(ctx, &call_token)) {
    // A special form was parsed, exit here
    return;
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

static void compiler_parse_number(CompilerContext *ctx) {
  double value = strtod(ctx->parser->previous.start, NULL);
  compiler_emit_constant(ctx, NUMBER_VAL(value));
}

static void compiler_parse_string(CompilerContext *ctx) {
  compiler_emit_constant(ctx,
                         OBJECT_VAL(mesche_object_make_string(ctx->vm,
                                                              ctx->parser->previous.start + 1,
                                                              ctx->parser->previous.length - 2)));
}

static void compiler_parse_literal(CompilerContext *ctx) {
  switch (ctx->parser->previous.kind) {
  case TokenKindNil: compiler_emit_byte(ctx, OP_NIL); break;
  case TokenKindTrue: compiler_emit_byte(ctx, OP_T); break;
  default: return; // We shouldn't hit this
  }
}

static void compiler_parse_expr(CompilerContext *ctx) {
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
  } else if (ctx->parser->current.kind == TokenKindSymbol) {
    compiler_advance(ctx);
    compiler_parse_identifier(ctx);
  }
}

static void compiler_end(CompilerContext *ctx) {
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
    .chunk = chunk,
    .local_count = 0,
    .scope_depth = 0
  };

  parser.had_error = false;
  parser.panic_mode = false;

  // Find the first legitimate token and then start parsing until
  // we reach the end of the source
  compiler_advance(&ctx);
  compiler_parse_block(&ctx, false);
  compiler_consume(&ctx, TokenKindEOF, "Expected end of expression.");

  compiler_end(&ctx);

  return !parser.had_error;
}
