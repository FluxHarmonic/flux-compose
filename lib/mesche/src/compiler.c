#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "compiler.h"
#include "disasm.h"
#include "mem.h"
#include "object.h"
#include "op.h"
#include "scanner.h"
#include "util.h"
#include "vm.h"

#define UINT8_COUNT (UINT8_MAX + 1)

// NOTE: Enable this for diagnostic purposes
#define DEBUG_PRINT_CODE

// Contains context for parsing tokens irrespective of the current compilation
// scope
typedef struct {
  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;
} Parser;

// Stores details relating to a local variable binding in a scope
typedef struct {
  Token name;
  int depth;
  bool is_captured;
} Local;

// Stores details relating to a captured variable in a parent scope
typedef struct {
  uint8_t index;
  bool is_local;
} Upvalue;

// Stores context for compilation at a particular scope
typedef struct CompilerContext {
  struct CompilerContext *parent;

  VM *vm;
  MescheMemory *mem;
  Parser *parser;
  Scanner *scanner;

  ObjectFunction *function;
  FunctionType function_type;

  Local locals[UINT8_COUNT];
  int local_count;
  int scope_depth;
  Upvalue upvalues[UINT8_COUNT];
} CompilerContext;

void mesche_compiler_mark_roots(void *target) {
  CompilerContext *ctx = (CompilerContext *)target;

  while (ctx != NULL) {
    mesche_mem_mark_object(ctx->vm, (Object *)ctx->function);
    ctx = ctx->parent;
  }
}

static void compiler_init_context(CompilerContext *ctx, CompilerContext *parent,
                                  FunctionType type) {
  if (parent != NULL) {
    ctx->parent = parent;
    ctx->vm = parent->vm;
    ctx->parser = parent->parser;
    ctx->scanner = parent->scanner;
  }

  // TODO: Assert if VM isn't set yet!  Should only happen at top-level scope.

  // Set up the compiler state for this scope
  ctx->function = mesche_object_make_function(ctx->vm, type);
  ctx->function_type = type;
  ctx->local_count = 0;
  ctx->scope_depth = 0;

  // Set up memory management
  ctx->vm->current_compiler = ctx;
  ctx->mem = &ctx->vm->mem;

  // Establish the first local slot
  Local *local = &ctx->locals[ctx->local_count++];
  local->depth = 0;
  local->is_captured = false;
  local->name.start = "";
  local->name.length = 0;
}

static void compiler_emit_byte(CompilerContext *ctx, uint8_t byte) {
  mesche_chunk_write(ctx->mem, &ctx->function->chunk, byte, ctx->parser->previous.line);
}

static void compiler_emit_bytes(CompilerContext *ctx, uint8_t byte1, uint8_t byte2) {
  compiler_emit_byte(ctx, byte1);
  compiler_emit_byte(ctx, byte2);
}

static void compiler_emit_return(CompilerContext *ctx) {
  compiler_emit_byte(ctx, OP_RETURN);
}

static ObjectFunction *compiler_end(CompilerContext *ctx) {
  ObjectFunction *function = ctx->function;
  compiler_emit_return(ctx);

#ifdef DEBUG_PRINT_CODE
  if (!ctx->parser->had_error) {
    mesche_disasm_function(function);
  }
#endif

  return function;
}

static uint8_t compiler_make_constant(CompilerContext *ctx, Value value) {
  int constant = mesche_chunk_constant_add(ctx->mem, &ctx->function->chunk, value);

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
  if (ctx->parser->panic_mode)
    return;

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
    if (ctx->parser->current.kind != TokenKindError)
      break;

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

static void compiler_parse_number(CompilerContext *ctx) {
  double value = strtod(ctx->parser->previous.start, NULL);
  compiler_emit_constant(ctx, NUMBER_VAL(value));
}

static void compiler_parse_string(CompilerContext *ctx) {
  compiler_emit_constant(
      ctx, OBJECT_VAL(mesche_object_make_string(ctx->vm, ctx->parser->previous.start + 1,
                                                ctx->parser->previous.length - 2)));
}

static void compiler_parse_keyword(CompilerContext *ctx) {
  compiler_emit_constant(
      ctx, OBJECT_VAL(mesche_object_make_keyword(ctx->vm, ctx->parser->previous.start + 1,
                                                 ctx->parser->previous.length - 1)));
}

static void compiler_parse_symbol_literal(CompilerContext *ctx) {
  compiler_emit_constant(
      ctx, OBJECT_VAL(mesche_object_make_symbol(ctx->vm, ctx->parser->previous.start,
                                                ctx->parser->previous.length)));
}

static void compiler_parse_literal(CompilerContext *ctx) {
  switch (ctx->parser->previous.kind) {
  case TokenKindNil:
    compiler_emit_byte(ctx, OP_NIL);
    break;
  case TokenKindTrue:
    compiler_emit_byte(ctx, OP_T);
    break;
  default:
    return; // We shouldn't hit this
  }
}

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
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static void compiler_add_local(CompilerContext *ctx, Token name) {
  if (ctx->local_count == UINT8_COUNT) {
    compiler_error(ctx, "Too many local variables defined in function.");
  }
  Local *local = &ctx->locals[ctx->local_count++];
  local->name = name; // No need to copy, will only be used during compilation
  local->depth = -1;  // The variable is uninitialized until assigned
  local->is_captured = false;
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

static int compiler_add_upvalue(CompilerContext *ctx, uint8_t index, bool is_local) {
  // Can we reuse an existing upvalue?
  int upvalue_count = ctx->function->upvalue_count;
  for (int i = 0; i < upvalue_count; i++) {
    Upvalue *upvalue = &ctx->upvalues[i];
    if (upvalue->index == index && upvalue->is_local == is_local) {
      return i;
    }
  }

  if (upvalue_count == UINT8_COUNT) {
    compiler_error(ctx, "Reached the limit of closures captured in one function.");
  }

  // Initialize the next upvalue slot
  ctx->upvalues[upvalue_count].is_local = is_local;
  ctx->upvalues[upvalue_count].index = index;
  return ctx->function->upvalue_count++;
}

static int compiler_resolve_upvalue(CompilerContext *ctx, Token *name) {
  // If there's no parent context then there's nothing to close over
  if (ctx->parent == NULL)
    return -1;

  // First try to resolve the variable as a local in the parent
  int local = compiler_resolve_local(ctx->parent, name);
  if (local != -1) {
    ctx->parent->locals[local].is_captured = true;
    return compiler_add_upvalue(ctx, (uint8_t)local, true);
  }

  // If we didn't find a local, look for a binding from a parent scope
  int upvalue = compiler_resolve_upvalue(ctx->parent, name);
  if (upvalue != -1) {
    return compiler_add_upvalue(ctx, (uint8_t)upvalue, false);
  }

  // No local or upvalue to bind to, assume global
  return -1;
}

static void compiler_local_mark_initialized(CompilerContext *ctx) {
  // If we're in global scope, don't do anything
  if (ctx->scope_depth == 0)
    return;

  // Mark the latest local variable as initialized
  ctx->locals[ctx->local_count - 1].depth = ctx->scope_depth;
}

static void compiler_declare_variable(CompilerContext *ctx) {
  // No need to declare a global, it will be dynamically resolved
  if (ctx->scope_depth == 0)
    return;

  // Start from the most recent local and work backwards to
  // find an existing variable with the same binding in this scope
  Token *name = &ctx->parser->previous;
  for (int i = ctx->local_count - 1; i >= 0; i--) {
    // If the local is in a different scope, stop looking
    Local *local = &ctx->locals[i];
    if (local->depth != -1 && local->depth < ctx->scope_depth)
      break;

    // In the same scope, is the identifier the same?
    if (compiler_identifiers_equal(name, &local->name)) {
      compiler_error(ctx, "Duplicate variable binding in 'let'");
      return;
    }
  }

  // Add a local binding for the name
  compiler_add_local(ctx, *name);
}

static uint8_t compiler_parse_symbol(CompilerContext *ctx, bool is_global) {
  // Declare the variable and exit if we're in a local scope
  compiler_declare_variable(ctx);
  if (!is_global && ctx->scope_depth > 0)
    return 0;

  Value new_string = OBJECT_VAL(mesche_object_make_string(ctx->vm, ctx->parser->previous.start,
                                                          ctx->parser->previous.length));

  // Reuse an existing constant for the same string if possible
  uint8_t constant = 0;
  bool value_found = false;
  Chunk *chunk = &ctx->function->chunk;
  for (int i = 0; i < chunk->constants.count; i++) {
    if (mesche_value_equalp(chunk->constants.values[i], new_string)) {
      constant = i;
      value_found = true;
      break;
    }
  }

  return value_found ? constant : compiler_make_constant(ctx, new_string);
}

static void compiler_parse_identifier(CompilerContext *ctx) {
  // Are we looking at a local variable?
  int local_index = compiler_resolve_local(ctx, &ctx->parser->previous);
  if (local_index != -1) {
    compiler_emit_bytes(ctx, OP_READ_LOCAL, (uint8_t)local_index);
  } else if ((local_index = compiler_resolve_upvalue(ctx, &ctx->parser->previous)) != -1) {
    // Found an upvalue
    compiler_emit_bytes(ctx, OP_READ_UPVALUE, (uint8_t)local_index);
  } else {
    uint8_t variable_constant = compiler_parse_symbol(ctx, true);
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

static void compiler_parse_set(CompilerContext *ctx) {
  compiler_consume(ctx, TokenKindSymbol, "Expected symbol after 'set!'");

  uint8_t instr = OP_SET_LOCAL;
  int arg = compiler_resolve_local(ctx, &ctx->parser->previous);

  // If there isn't a local, use a global variable instead
  if (arg == -1) {
    arg = compiler_parse_symbol(ctx, true);
    instr = OP_SET_GLOBAL;
  } else if ((arg = compiler_resolve_upvalue(ctx, &ctx->parser->previous)) != -1) {
    instr = OP_SET_UPVALUE;
  }

  compiler_parse_expr(ctx);
  compiler_emit_bytes(ctx, instr, arg);

  compiler_consume(ctx, TokenKindRightParen, "Expected closing paren.");
}

static void compiler_begin_scope(CompilerContext *ctx) {
  ctx->scope_depth++;
}

static void compiler_end_scope(CompilerContext *ctx) {
  // Pop all local variables from the previous scope while closing any upvalues
  // that have been captured inside of it
  ctx->scope_depth--;
  uint8_t local_count = 0;
  while (ctx->local_count > 0 && ctx->locals[ctx->local_count - 1].depth > ctx->scope_depth) {
    local_count++;
    if (ctx->locals[ctx->local_count - 1].is_captured) {
      compiler_emit_byte(ctx, OP_CLOSE_UPVALUE);
    } else {
      compiler_emit_bytes(ctx, OP_POP_SCOPE, 1);
    }
    ctx->local_count--;
  }

  // Pop all of the locals off of the scope, retaining the final expression result
  compiler_emit_bytes(ctx, OP_POP_SCOPE, local_count);
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
    compiler_parse_symbol(ctx, false);
    compiler_parse_expr(ctx);
    compiler_define_variable(ctx, 0 /* Irrelevant, this is local */);

    compiler_consume(ctx, TokenKindRightParen, "Expected right paren to end binding pair");
  }

  compiler_parse_block(ctx, true);
  compiler_end_scope(ctx);
}

static void compiler_parse_lambda_inner(CompilerContext *ctx) {
  // Create a new compiler context for parsing the function body
  CompilerContext func_ctx;
  compiler_init_context(&func_ctx, ctx, TYPE_FUNCTION);
  compiler_begin_scope(&func_ctx);

  bool in_keyword_list = false;
  for (;;) {
    // Try to parse each argument until we reach a closing paren
    if (func_ctx.parser->current.kind == TokenKindRightParen) {
      compiler_consume(&func_ctx, TokenKindRightParen,
                       "Expected right paren to end argument list.");
      break;
    }

    if (func_ctx.parser->current.kind == TokenKindKeyword) {
      compiler_consume(&func_ctx, TokenKindKeyword,
                       "Expected :keys keyword to start keyword list.");
      in_keyword_list = true;
    }

    if (!in_keyword_list) {
      // Increase the function argument count (arity)
      func_ctx.function->arity++;
      if (func_ctx.function->arity > 255) {
        compiler_error_at_current(&func_ctx, "Function cannot have more than 255 parameters.");
      }

      // Parse the argument
      // TODO: Ensure a symbol comes next
      compiler_advance(&func_ctx);
      uint8_t constant = compiler_parse_symbol(&func_ctx, false);
      compiler_define_variable(&func_ctx, constant);
    } else {
      compiler_advance(&func_ctx);

      // Check if there is a default value pair
      bool has_default = false;
      if (func_ctx.parser->current.kind == TokenKindLeftParen) {
        compiler_advance(&func_ctx);
        has_default = true;
      }

      // Parse the keyword name and define it as a local variable
      uint8_t constant = compiler_parse_symbol(&func_ctx, false);
      compiler_define_variable(&func_ctx, constant);

      // Parse the default if we're expecting one
      uint8_t default_constant = 0;
      if (has_default) {
        compiler_consume(&func_ctx, TokenKindRightParen,
                         "Expected right paren after keyword default value.");
      }

      // Add the keyword definition to the function
      KeywordArgument keyword_arg = {
          .name = mesche_object_make_string(ctx->vm, ctx->parser->previous.start,
                                            ctx->parser->previous.length),
          .default_index = default_constant,
      };

      mesche_object_function_keyword_add(ctx->mem, func_ctx.function, keyword_arg);
    }
  }

  // Parse body
  compiler_parse_block(&func_ctx, true);

  // Get the parsed function and store it in a constant
  ObjectFunction *function = compiler_end(&func_ctx);
  compiler_emit_bytes(ctx, OP_CLOSURE, compiler_make_constant(ctx, OBJECT_VAL(function)));

  // Write out the references to each upvalue as arguments to OP_CLOSURE
  for (int i = 0; i < function->upvalue_count; i++) {
    compiler_emit_byte(ctx, func_ctx.upvalues[i].is_local ? 1 : 0);
    compiler_emit_byte(ctx, func_ctx.upvalues[i].index);
  }

  // Let the VM know we're back to the parent compiler
  ctx->vm->current_compiler = ctx;
}

static void compiler_parse_lambda(CompilerContext *ctx) {
  // Consume the leading paren and let the shared lambda parser take over
  compiler_consume(ctx, TokenKindLeftParen, "Expected left paren to begin argument list.");
  compiler_parse_lambda_inner(ctx);
}

static void compiler_parse_define(CompilerContext *ctx) {
  // The next symbol should either be a symbol or an open paren to define a function
  bool is_func = false;
  if (ctx->parser->current.kind == TokenKindLeftParen) {
    compiler_consume(ctx, TokenKindLeftParen, "Expected left paren after 'define'");
    is_func = true;
  }

  compiler_consume(ctx, TokenKindSymbol, "Expected symbol after 'define'");

  uint8_t variable_constant = variable_constant = compiler_parse_symbol(ctx, true);
  if (is_func) {
    // Let the lambda parser take over
    compiler_parse_lambda_inner(ctx);
  } else {
    // Parse a normal expression
    compiler_parse_expr(ctx);
    compiler_consume(ctx, TokenKindRightParen, "Expected closing paren.");
  }

  // TODO: Only allow defines at the top of let/lambda bodies
  compiler_define_variable(ctx, variable_constant);
}

static int compiler_emit_jump(CompilerContext *ctx, uint8_t instruction) {
  // Write out the two bytes that will be patched once the jump target location
  // is determined
  compiler_emit_byte(ctx, instruction);
  compiler_emit_byte(ctx, 0xff);
  compiler_emit_byte(ctx, 0xff);

  return ctx->function->chunk.count - 2;
}

static void compiler_patch_jump(CompilerContext *ctx, int offset) {
  // We offset by -2 to adjust for the size of the jump offset itself
  int jump = ctx->function->chunk.count - offset - 2;

  if (jump > UINT16_MAX) {
    compiler_error(ctx, "Attempting to emit jump that is larger than possible jump size.");
  }

  // Place the two bytes with the jump delta
  ctx->function->chunk.code[offset] = (jump >> 8) & 0xff;
  ctx->function->chunk.code[offset + 1] = jump & 0xff;
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

static void compiler_parse_operator_call(CompilerContext *ctx, Token *call_token, uint8_t operand_count) {
  TokenKind operator= call_token->kind;
  switch (operator) {
  case TokenKindPlus:
    compiler_emit_byte(ctx, OP_ADD);
    break;
  case TokenKindMinus:
    compiler_emit_byte(ctx, OP_SUBTRACT);
    break;
  case TokenKindStar:
    compiler_emit_byte(ctx, OP_MULTIPLY);
    break;
  case TokenKindSlash:
    compiler_emit_byte(ctx, OP_DIVIDE);
    break;
  case TokenKindAnd:
    compiler_emit_byte(ctx, OP_AND);
    break;
  case TokenKindOr:
    compiler_emit_byte(ctx, OP_OR);
    break;
  case TokenKindNot:
    compiler_emit_byte(ctx, OP_NOT);
    break;
  case TokenKindList:
    compiler_emit_bytes(ctx, OP_LIST, operand_count);
    break;
  case TokenKindCons:
    compiler_emit_byte(ctx, OP_CONS);
    break;
  case TokenKindEqv:
    compiler_emit_byte(ctx, OP_EQV);
    break;
  case TokenKindEqual:
    compiler_emit_byte(ctx, OP_EQUAL);
    break;
  case TokenKindDisplay:
    compiler_emit_byte(ctx, OP_DISPLAY);
    break;
  default:
    return; // We shouldn't hit this
  }
}

static bool compiler_parse_special_form(CompilerContext *ctx, Token *call_token) {
  TokenKind operator= call_token->kind;
  switch (operator) {
  case TokenKindBegin:
    compiler_parse_block(ctx, true);
    break;
  case TokenKindDefine:
    compiler_parse_define(ctx);
    break;
  case TokenKindSet:
    compiler_parse_set(ctx);
    break;
  case TokenKindLet:
    compiler_parse_let(ctx);
    break;
  case TokenKindIf:
    compiler_parse_if(ctx);
    break;
  case TokenKindLambda:
    compiler_parse_lambda(ctx);
    break;
  default:
    return false; // No special form found
  }

  return true;
}

static void compiler_parse_quoted_list(CompilerContext *ctx) {
  bool is_backquote = ctx->parser->previous.kind == TokenKindBackquote;

  // Ensure the open paren is there
  compiler_consume(ctx, TokenKindLeftParen, "Expected left paren after quote.");

  uint8_t item_count = 0;
  for (;;) {
    // Bail out when we hit the closing parentheses
    if (ctx->parser->current.kind == TokenKindRightParen) {
      // Emit the list operation
      compiler_advance(ctx);
      compiler_emit_bytes(ctx, OP_LIST, item_count);
      return;
    }

    // Parse certain token types differently in quoted lists
    switch (ctx->parser->current.kind) {
    case TokenKindSymbol:
      compiler_advance(ctx);
      compiler_parse_symbol_literal(ctx);
      break;
    case TokenKindLeftParen:
      compiler_parse_quoted_list(ctx);
      break;
    case TokenKindUnquote:
      // Evaluate whatever the next expression is
      compiler_advance(ctx);

      if (ctx->parser->current.kind == TokenKindSplice) {
        PANIC("Splicing is not currently supported.\n");
      }

      if (!is_backquote) {
        compiler_error(ctx, "Cannot use unquote in a non-backquoted expression.");
        compiler_parse_quoted_list(ctx);
      } else {
        compiler_parse_expr(ctx);
      }
      break;
    case TokenKindPlus:
    case TokenKindMinus:
    case TokenKindSlash:
    case TokenKindStar:
      compiler_advance(ctx);
      compiler_parse_symbol_literal(ctx);
      break;
    default: compiler_parse_expr(ctx);
    }

    item_count++;
  }
}

static void compiler_parse_list(CompilerContext *ctx) {
  // Try to find the call target (this could be an expression!)
  Token call_token = ctx->parser->current;
  bool is_call = false;

  // Possibilities
  // - Primitive command with its own opcode
  // - Special form that has non-standard call semantics
  // - Symbol in first position
  // - Raw lambda in first position
  // - Expression that evaluates to lambda
  // In the latter 3 cases, compiler the callee before the arguments

  // Evaluate the first expression if it's not an operator
  if (call_token.kind == TokenKindSymbol || call_token.kind == TokenKindLeftParen) {
    compiler_parse_expr(ctx);
    is_call = true;
  } else {
    compiler_advance(ctx);
    if (compiler_parse_special_form(ctx, &call_token)) {
      // A special form was parsed, exit here
      return;
    }
  }

  // Parse argument expressions until we reach a right paren
  uint8_t arg_count = 0;
  bool in_keyword_args = false;
  for (;;) {
    // Bail out when we hit the closing parentheses
    if (ctx->parser->current.kind == TokenKindRightParen) {
      if (is_call == false) {
        // Compile the primitive operator
        compiler_parse_operator_call(ctx, &call_token, arg_count);
      } else {
        // Emit the call operation
        compiler_emit_bytes(ctx, OP_CALL, arg_count);
      }

      // Consume the right paren and exit
      compiler_consume(ctx, TokenKindRightParen, "Expected closing paren.");
      return;
    }

    // Is it a keyword argument?
    if (!in_keyword_args && ctx->parser->current.kind == TokenKindKeyword) {
      // We're only looking for keyword arguments from this point on
      in_keyword_args = true;
    }

    if (in_keyword_args) {
      // Parse the keyword and value
      compiler_consume(ctx, TokenKindKeyword, "Expected keyword.");
      compiler_parse_keyword(ctx);
      compiler_parse_expr(ctx);
      arg_count++; // Add one more argument for the value we just parsed
    } else {
      // Compile next positional parameter
      compiler_parse_expr(ctx);
    }

    if (arg_count == 255) {
      compiler_error(ctx, "Cannot pass more than 255 arguments in a function call.");
    }

    arg_count++;
  }
}

void (*ParserFunc)(CompilerContext *ctx);

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
  if (ctx->parser->current.kind == TokenKindQuote ||
      ctx->parser->current.kind == TokenKindBackquote) {
    compiler_advance(ctx);
    compiler_parse_quoted_list(ctx);
    return;
  }

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
  } else if (ctx->parser->current.kind == TokenKindKeyword) {
    compiler_advance(ctx);
    compiler_parse_keyword(ctx);
  } else if (ctx->parser->current.kind == TokenKindSymbol) {
    compiler_advance(ctx);
    compiler_parse_identifier(ctx);
  } else {
    PANIC("Unexpected token kind: %d\n", ctx->parser->current.kind);
  }
}

ObjectFunction *mesche_compile_source(VM *vm, const char *script_source) {
  Parser parser;
  Scanner scanner;
  mesche_scanner_init(&scanner, script_source);

  // Set up the context
  CompilerContext ctx = {
      .vm = vm,
      .parser = &parser,
      .scanner = &scanner,
  };
  compiler_init_context(&ctx, NULL, TYPE_SCRIPT);

  // Reset parser error state
  parser.had_error = false;
  parser.panic_mode = false;

  // Find the first legitimate token and then start parsing until
  // we reach the end of the source
  compiler_advance(&ctx);
  compiler_parse_block(&ctx, false);
  compiler_consume(&ctx, TokenKindEOF, "Expected end of expression.");

  // Clear the VM's pointer to this compiler
  vm->current_compiler = NULL;

  // Retrieve the final function and return it if there were no parse errors
  ObjectFunction *function = compiler_end(&ctx);
  return parser.had_error ? NULL : function;
}
