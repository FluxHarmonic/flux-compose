#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "flux.h"
#include "flux-internal.h"

void *script_value_buffer = NULL;

Vector token_vector = NULL;
Vector parse_vector = NULL;

#define TOKEN_BUFFER_INITIAL_SIZE 1024
#define PARSE_BUFFER_INITIAL_SIZE 1024
#define VALUE_BUFFER_INITIAL_SIZE 2048

// TODO: Use an option or compiler directive to disable these

/* #define TOKEN_LOG(ptr, message, ...) \ */
/*   flux_log_mem(ptr, "TOKEN | ");     \ */
/*   flux_log(message __VA_OPT__(,) __VA_ARGS__); */

/* #define PARSE_LOG(ptr, message, ...) \ */
/*   flux_log_mem(ptr, "PARSE | ");     \ */
/*   flux_log(message __VA_OPT__(,) __VA_ARGS__); */

/* #define EVAL_LOG(ptr, message, ...)  \ */
/*   flux_log_mem(ptr, "EVAL  | ");     \ */
/*   flux_log(message __VA_OPT__(,) __VA_ARGS__); */

#define TOKEN_LOG(ptr, message, ...) {}
#define PARSE_LOG(ptr, message, ...) {}
#define EVAL_LOG(ptr, message, ...) {}

typedef enum {
  TSTATE_NONE,
  TSTATE_SYMBOL,
  TSTATE_KEYWORD,
  TSTATE_STRING,
  TSTATE_NUMBER,
} TokenState;

#define token_reset_state(token_state, str_ptr, str_buffer)  \
  str_ptr = &str_buffer[0];                                  \
  *str_ptr = '\0';                                           \
  token_state = TSTATE_NONE;

#define FLAG_MINUS  1 << 31
#define FLAG_QUOTED 1 << 31

int flux_flag_check(int value, int bit_mask) {
  return (value & bit_mask) != 0;
}

int is_state(int state, int expected) {
  // State flags take up the final 4 bits
  static int flag_mask = 0b00001111111111111111111111111111;
  return expected == (state & flag_mask);
}

void token_add_char(char **str_ptr, char c) {
  // TODO: Check buffer bounds
  **str_ptr = c;
  (*str_ptr)++;
  **str_ptr = '\0';
}

size_t flux_script_token_size(void *item) {
  TokenHeader* token = item;

  switch (token->kind) {
  case TokenKindParen:
    return sizeof(TokenParen);
  case TokenKindInteger:
    return sizeof(TokenInteger);
  case TokenKindFloat:
    return sizeof(TokenFloat);
  case TokenKindString:
    return sizeof(TokenString) + ((TokenString *)token)->length + 1;
  case TokenKindSymbol:
    return sizeof(TokenSymbol) + ((TokenSymbol *)token)->length + 1;
  case TokenKindKeyword:
    return sizeof(TokenKeyword) + ((TokenKeyword *)token)->length + 1;
  default:
    PANIC("Unhandled token type: %d\n", token->kind);
  }
}

// Tokenize the script source for the provided file stream
Vector flux_script_tokenize(FILE *script_file) {
  if (token_vector == NULL) {
    token_vector = flux_vector_create(TOKEN_BUFFER_INITIAL_SIZE, flux_script_token_size);
    TOKEN_LOG(token_vector, "Allocated %d bytes for token buffer\n", TOKEN_BUFFER_INITIAL_SIZE);
  } else {
    flux_vector_reset(token_vector);
  }

  VectorCursor token_cursor;
  flux_vector_cursor_init(token_vector, &token_cursor);

  short c = 0;
  int token_state = TSTATE_NONE;

  char str_buffer[2048]; // TODO: This may need to be adjustable
  char *str_ptr = &str_buffer[0];

  TOKEN_LOG(token_vector->start_item, "--- TOKENIZATION START ---\n");

  while (!feof(script_file)) {
    c = fgetc(script_file);

    // Are we in a string?
    if (is_state(token_state, TSTATE_STRING)) {
      if (c == '"') {
        // TODO: Is the previous char a backslash?
        // Save the string token
        TokenString str_token;
        str_token.header.kind = TokenKindString;
        str_token.length = str_ptr - &str_buffer[0];
        strcpy(str_token.string, &str_buffer[0]);

        TokenHeader *token = flux_vector_push(&token_cursor, &str_token);
        TOKEN_LOG(token, "String - %s (length: %d)\n", str_token.string, str_token.length);
        token_reset_state(token_state, str_ptr, str_buffer);
      } else {
        token_add_char(&str_ptr, c);
      }
    } else if (is_state(token_state, TSTATE_SYMBOL)) {
      if (c == '-' || c == '_' || isalnum(c)) {
        token_add_char(&str_ptr, c);
      } else {
        // End the symbol
        TokenSymbol symbol_token;
        symbol_token.header.kind = TokenKindSymbol;
        symbol_token.length = str_ptr - &str_buffer[0];
        symbol_token.is_quoted = flux_flag_check(token_state, FLAG_QUOTED);
        strcpy(symbol_token.string, &str_buffer[0]);

        // Put the character back on the stream to read it again
        ungetc(c, script_file);

        TokenHeader *token = flux_vector_push(&token_cursor, &symbol_token);
        TOKEN_LOG(token, "Symbol - %s (length: %d)\n", symbol_token.string, symbol_token.length);
        token_reset_state(token_state, str_ptr, str_buffer);
      }
    } else if (is_state(token_state, TSTATE_KEYWORD)) {
      if (c == '-' || c == '_' || isalnum(c)) {
        token_add_char(&str_ptr, c);
      } else {
        // End the keyword
        TokenKeyword keyword_token;
        keyword_token.header.kind = TokenKindKeyword;
        keyword_token.length = str_ptr - &str_buffer[0];
        strcpy(keyword_token.string, &str_buffer[0]);

        // Put the character back on the stream to read it again
        ungetc(c, script_file);

        TokenHeader *token = flux_vector_push(&token_cursor, &keyword_token);
        TOKEN_LOG(token, "Keyword - :%s (length: %d)\n", keyword_token.string, keyword_token.length);
        token_reset_state(token_state, str_ptr, str_buffer);
      }
    } else if (is_state(token_state, TSTATE_NUMBER)) {
      if (isdigit(c)) {
        token_add_char(&str_ptr, c);
      } else {
        // End the keyword
        TokenInteger integer_token;
        integer_token.header.kind = TokenKindInteger;
        integer_token.number = atoi(str_buffer);
        if (flux_flag_check(token_state, FLAG_MINUS)) {
          integer_token.number = -1 * integer_token.number;
        }

        // Put the character back on the stream to read it again
        ungetc(c, script_file);

        TokenHeader *token = flux_vector_push(&token_cursor, &integer_token);
        TOKEN_LOG(token, "Integer - %d\n", integer_token.number);
        token_reset_state(token_state, str_ptr, str_buffer);
      }
    } else {
      if (c == '(') {
        TokenParen paren;
        paren.header.kind = TokenKindParen;
        paren.is_open = 1;

        TokenHeader *token = flux_vector_push(&token_cursor, &paren);
        TOKEN_LOG(token, "Open Paren\n");
        token_reset_state(token_state, str_ptr, str_buffer);
      } else if (c == ')') {
        // Are we in a string?
        if (token_state == TSTATE_STRING) {
          continue;
        }

        TokenParen paren;
        paren.header.kind = TokenKindParen;
        paren.is_open = 0;

        TokenHeader *token = flux_vector_push(&token_cursor, &paren);
        TOKEN_LOG(token, "Close Paren\n");

        token_reset_state(token_state, str_ptr, str_buffer);
      } else if (c == '"') {
        token_state = TSTATE_STRING;
      } else if (c == ':') {
        token_state = TSTATE_KEYWORD;
      } else if (c == '\'') {
        token_state = TSTATE_SYMBOL | FLAG_QUOTED;
      } else if (isalpha(c)) {
        token_add_char(&str_ptr, c);
        token_state = TSTATE_SYMBOL;
      } else if (c == '-') {
        token_state = TSTATE_NUMBER | FLAG_MINUS;
      } else if (isdigit(c)) {
        token_add_char(&str_ptr, c);
        token_state = TSTATE_NUMBER;
      }
    }
  }

  TOKEN_LOG(token_vector->start_item, "Finished tokenization!\n");

  return token_vector;
}

typedef enum {
  PSTATE_NONE,
} ParseState;

size_t flux_script_value_size(ValueHeader *value) {
  switch(value->kind) {
  case ValueKindInteger:
    return sizeof(ValueInteger);
  case ValueKindString:
    return sizeof(ValueString) + ((ValueString*)value)->length + 1;
  default:
    PANIC("Unhandled value type: %d\n", value->kind);
  }
}

size_t flux_script_expr_size(void *item) {
  ExprHeader *expr = item;

  switch (expr->kind) {
  case ExprKindList:
    PARSE_LOG(expr, "Item is List (presumed size: %lu + %lu)\n", sizeof(ExprList), ((ExprList *)expr)->items.buffer_usage);
    return sizeof(ExprList) + ((ExprList *)expr)->items.buffer_usage;
  case ExprKindSymbol:
    PARSE_LOG(expr, "Item is Symbol\n");
    return sizeof(ExprSymbol) + ((ExprSymbol *)expr)->length + 1;
  case ExprKindKeyword:
    PARSE_LOG(expr, "Item is Keyword\n");
    return sizeof(ExprKeyword) + ((ExprKeyword *)expr)->length + 1;
  case ExprKindString:
    PARSE_LOG(expr, "Item is String\n");
    return sizeof(ExprString) + ((ExprString *)expr)->length + 1;
  case ExprKindInteger:
    PARSE_LOG(expr, "Item is Integer\n");
    return sizeof(ExprInteger);
  default:
    PANIC("Unhandled expr type: %d\n", expr->kind);
  }
}

ExprList *flux_script_parse_list(VectorCursor *token_cursor, VectorCursor *list_cursor) {
  PARSE_LOG(list_cursor->vector, "Parse list starting at %x...\n", &list_cursor->vector->start_item);

  while (flux_vector_cursor_has_next(token_cursor)) {
    TokenHeader *token = flux_vector_cursor_next(token_cursor);
    PARSE_LOG(list_cursor->vector, "Parser got token %d (%x)\n", token->kind, token);

    // Parse sub-lists
    if (token->kind == TokenKindParen) {
      if (((TokenParen*)token)->is_open) {
        PARSE_LOG(list_cursor->vector, "START sub-list --\n");

        // Start a new temporary list that will be copied later
        ExprList temp_sub_list;
        temp_sub_list.header.kind = ExprKindList;
        flux_vector_init(&temp_sub_list.items, flux_script_expr_size);

        // Push the new list onto the parent vector and set up a cursor
        ExprList *sub_list = flux_vector_push(list_cursor, &temp_sub_list);
        sub_list->items.start_item = sub_list + 1;
        PARSE_LOG(sub_list, "Created sub-vector for nested list (vector at %x)\n", &sub_list->items);
        VectorCursor sub_list_cursor;
        flux_vector_cursor_init(&sub_list->items, &sub_list_cursor);

        PARSE_LOG(&sub_list->items, "This is the sub-list vector (func: %x)\n", sub_list->items.item_size_func);
        PARSE_LOG(&sub_list->items, "The real size func location is %x\n", flux_script_expr_size);

        // Parse the list contents recursively.  The sub-cursor will tell us
        // where to resume the parent list's data once this function completes.
        PARSE_LOG(list_cursor->vector, "Begin sub-list parse starting at %x...\n", list_cursor->current_item);
        flux_script_parse_list(token_cursor, &sub_list_cursor);

        // Trace where we're headed next
        PARSE_LOG(list_cursor->current_item, "Outer list will resume at %x\n", sub_list_cursor.current_item);

        // Set the outer cursor to be where the inner cursor left off
        list_cursor->current_item = sub_list_cursor.current_item;
      } else {
        PARSE_LOG(list_cursor->current_item, "END list, length %d...\n", list_cursor->vector->length);
        return (ExprList*)((uintptr_t)list_cursor->vector - sizeof(ExprHeader));
      }
    } else if (token->kind == TokenKindSymbol) {
      ExprSymbol symbol;
      PARSE_LOG(list_cursor->current_item, "Setting symbol: %s\n", ((TokenSymbol *)token)->string);
      symbol.header.kind = ExprKindSymbol;
      symbol.length = ((TokenSymbol*)token)->length;
      strcpy(symbol.name, ((TokenSymbol*)token)->string);
      symbol.is_quoted = ((TokenSymbol*)token)->is_quoted;

      ExprSymbol *final_symbol = flux_vector_push(list_cursor, &symbol);
      PARSE_LOG(final_symbol, "Pushed symbol: %s\n", final_symbol->name);
    } else if (token->kind == TokenKindKeyword) {
      ExprKeyword keyword;
      PARSE_LOG(list_cursor->current_item, "Setting keyword: %s\n", ((TokenKeyword *)token)->string);
      keyword.header.kind = ExprKindKeyword;
      keyword.length = ((TokenKeyword*)token)->length;
      strcpy(keyword.name, ((TokenKeyword*)token)->string);

      flux_vector_push(list_cursor, &keyword);
    } else if (token->kind == TokenKindInteger) {
      ExprInteger integer;
      PARSE_LOG(list_cursor->current_item, "Setting integer: %d\n", ((TokenInteger *)token)->number);
      integer.header.kind = ExprKindInteger;
      integer.number = ((TokenInteger *)token)->number;

      flux_vector_push(list_cursor, &integer);
    } else if (token->kind == TokenKindString) {
      ExprString string;
      PARSE_LOG(list_cursor->current_item, "Setting string: %s\n", ((TokenString *)token)->string);
      string.header.kind = ExprKindString;
      string.length = ((TokenString*)token)->length;
      strcpy(string.string, ((TokenString*)token)->string);

      ExprString *final_string = flux_vector_push(list_cursor, &string);
      PARSE_LOG(final_string, "Pushed string: %s\n", final_string->string);
    } else if (token->kind != TokenKindNone) {
      // If we see any unusual token kind, bail out
      PANIC("Parser received unexpected TokenKind %d\n", token->kind);
    }
  }

  // At this point, assume we've reached TokenKindNone
  PARSE_LOG(list_cursor->current_item, "EXIT list parse\n");
}

Vector flux_script_parse(Vector token_vector) {
  // Allocate the parse vector if necessary
  if (parse_vector == NULL) {
    parse_vector = flux_vector_create(PARSE_BUFFER_INITIAL_SIZE, flux_script_expr_size);
    PARSE_LOG(parse_vector, "Allocated %d bytes for parse buffer\n", PARSE_BUFFER_INITIAL_SIZE);
  } else {
    flux_vector_reset(parse_vector);
  }

  // Set the token cursor
  VectorCursor token_cursor;
  flux_vector_cursor_init(token_vector, &token_cursor);

  PARSE_LOG(parse_vector, "--- PARSING START ---\n");

  // Parse the top-level list, this will parse everything recursively
  VectorCursor list_cursor;
  flux_vector_cursor_init(parse_vector, &list_cursor);
  flux_script_parse_list(&token_cursor, &list_cursor);

  PARSE_LOG(list_cursor.current_item, "FINISHED parsing top-level!\n");

  return parse_vector;
}

ValueHeader *flux_script_value_next(ValueCursor *value_cursor) {
  ValueHeader *value = value_cursor->current;

  EVAL_LOG(value_cursor->current, "Next value requested, current kind: %d\n", value_cursor->current->kind);

  switch (value->kind) {
  case ValueKindNone:
    // We're already at the end of the value array
    // TODO: Signal this somehow?
    EVAL_LOG(value, "Can't move forward, already at the last value!\n");
    break;
  case ValueKindInteger:
    value += 1;
    break;
  case ValueKindFloat:
    value += 1;
    break;
  case ValueKindString:
    value = (ValueHeader *)((uintptr_t)value + sizeof(ValueString) + ((ValueString*)value)->length + 1);
    break;
  default:
    PANIC("Unhandled value type: %d\n", value->kind);
  }

  // Update the cursor with the current location
  value_cursor->current = value;

  EVAL_LOG(value_cursor->current, "Next value is %d\n", value_cursor->current->kind);

  return value;
}

ValueHeader *flux_script_value_copy(ValueHeader *value, ValueCursor *value_cursor) {
  ValueHeader *new_value = value_cursor->current;;

  switch (value->kind) {
  case ValueKindInteger:
    EVAL_LOG(value, "Copying integer %d to %x\n", ((ValueInteger *)value)->value, new_value);
    memcpy(new_value, value, sizeof(ValueInteger));
    break;
  case ValueKindString:
    EVAL_LOG(value, "Copying string \"%s\" to %x\n", ((ValueString *)value)->string, new_value);
    memcpy(new_value, value, sizeof(ValueString) + ((ValueString *)value)->length + 1);
    break;
  }

  // Prepare the next cursor location
  flux_script_value_next(value_cursor);

  return new_value;
}

ValueHeader *flux_script_func_add(VectorCursor *list_cursor, ValueCursor *value_cursor) {
  int i;
  int sum = 0;

  // Iterate over the remaining expressions, evaluate them, and do some work
  // TODO: Don't limit the numbre of inputs
  for (i = 0; i < 2; i++) {
    EVAL_LOG(list_cursor->current_item, "INSIDE LOOP: %d %d\n", i, list_cursor->current->kind);
    flux_vector_cursor_next(list_cursor);
    ValueHeader *result = flux_script_eval_expr(list_cursor->current_item);
    // TODO: Verify that it's an integer
    sum += ((ValueInteger *)result)->value;
  }

  // Ask for a new value slot
  // TODO: Don't assume that we already know the slot!
  ValueInteger *result = (ValueInteger *)value_cursor->current;
  result->header.kind = ValueKindInteger;
  result->value = sum;

  return (ValueHeader *)result;
}

ValueHeader *flux_script_eval_expr(ExprHeader *expr) {
  ValueCursor value_cursor;
  VectorCursor arg_cursor;

  // Allocate the parse buffer if necessary
  if (script_value_buffer == NULL) {
    script_value_buffer = flux_memory_alloc(VALUE_BUFFER_INITIAL_SIZE);
    EVAL_LOG(script_value_buffer, "Allocated %d bytes for value buffer\n", VALUE_BUFFER_INITIAL_SIZE);
  }

  value_cursor.current = script_value_buffer;
  value_cursor.current->kind = 0;

  EVAL_LOG(value_cursor.current, "Eval expr kind %d\n", expr->kind);

  switch (expr->kind) {
  case ExprKindInteger:
    /* ValueInteger int_value; */
    /* int_value.header.kind = ValueKindInteger; */
    /* int_value.value = ((ExprInteger *)expr)->number; */
    // TODO: Fix this!
    return NULL;
  case ExprKindString:
    /* ValueString str_value; */
    /* str_value.header.kind = ValueKindString; */
    /* str_value.length = ((ExprString*)token)->length; */
    /* strcpy(str_value.string, ((ExprString*)token)->string); */
    return NULL;
  case ExprKindList:
    // Initialize the cursor for the expression

    // First of all, grab the symbol at the beginning of the list
    flux_vector_cursor_init(&((ExprList*)expr)->items, &arg_cursor);
    ExprHeader *call_symbol = flux_vector_cursor_next(&arg_cursor);

    // Look up symbol
    // Make sure it's a funciton pointer
    // Invoke it with a cursor for the ExprList

    // TODO: Check that it's a symbol
    EVAL_LOG(call_symbol, "Call symbol kind: %d\n", call_symbol->kind);

    if (call_symbol->kind == ExprKindSymbol) {
      char *symbol_name = ((ExprSymbol *)call_symbol)->name;
      EVAL_LOG(call_symbol, "Call expr with symbol: %s\n", symbol_name);

      if (strcmp(symbol_name, "add") == 0) {
        return flux_script_func_add(&arg_cursor, &value_cursor);
      } else if (strcmp(symbol_name, "show-preview-window") == 0) {
        return flux_graphics_func_show_preview_window(&arg_cursor, &value_cursor);
      } else if (strcmp(symbol_name, "circle") == 0) {
        return flux_graphics_func_circle(&arg_cursor, &value_cursor);
      }
    } else {
      PANIC("Call expression has expr of kind %d in first position!\n", call_symbol->kind);
    }

    return NULL;
  }

  // TODO: We probably should never get here
  return NULL;
}

ValueHeader *flux_script_eval(FILE *script_file) {
  Vector token_vector = flux_script_tokenize(script_file);
  if (token_vector != NULL && token_vector->length > 0) {
    // TODO: Loop over every expression at top-level and eval individually
    ValueCursor value_cursor;
    VectorCursor list_cursor;
    Vector result = flux_script_parse(token_vector);
    flux_vector_cursor_init(result, &list_cursor);

    ExprHeader *next_expr = flux_vector_cursor_next(&list_cursor);
    return flux_script_eval_expr(next_expr);
  }

  // TODO: Better return type
  return NULL;
}

ValueHeader *flux_script_eval_string(char *script_string) {
  FILE *stream = flux_file_from_string(script_string);
  flux_script_eval(stream);
}
