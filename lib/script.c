#include <stdio.h>
#include <string.h>
#include "flux.h"
#include "flux-internal.h"

void *script_token_buffer = NULL;
void *script_parse_buffer = NULL;

#define TOKEN_BUFFER_INITIAL_SIZE 1024
#define PARSE_BUFFER_INITIAL_SIZE 1024

// TODO: Use an option or compiler directive to disable these

#define TOKEN_LOG(ptr, message, ...) \
  flux_log_mem(ptr, "TOKEN | ");     \
  flux_log(message __VA_OPT__(,) __VA_ARGS__);

#define PARSE_LOG(ptr, message, ...) \
  flux_log_mem(ptr, "PARSE | ");     \
  flux_log(message __VA_OPT__(,) __VA_ARGS__);

typedef enum {
  TSTATE_NONE,
  TSTATE_SYMBOL,
  TSTATE_KEYWORD,
  TSTATE_STRING,
  TSTATE_NUMBER,
} TokenState;

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

// Return a pointer to the next token
TokenHeader *flux_script_token_next(TokenCursor *token_cursor) {
  TokenHeader *token = token_cursor->current;

  TOKEN_LOG(token_cursor->current, "Next token requested, current kind: %d\n", token_cursor->current->kind);

  switch (token->kind) {
  case TokenKindNone:
    // We're already at the end of the token array
    // TODO: Signal this somehow?
    TOKEN_LOG(token, "Can't move forward, already at the last token!\n");
    break;
  case TokenKindParen:
    token += sizeof(TokenParen);
    break;
  case TokenKindInteger:
    token += sizeof(TokenInteger);
    break;
  case TokenKindFloat:
    token += sizeof(TokenFloat);
    break;
  case TokenKindString:
    token += sizeof(TokenString) + ((TokenString *)token)->length + 1;
    break;
  case TokenKindSymbol:
    token += sizeof(TokenSymbol) + ((TokenSymbol *)token)->length + 1;
    break;
  case TokenKindKeyword:
    token += sizeof(TokenKeyword) + ((TokenKeyword *)token)->length + 1;
    break;
  default:
    PANIC("Unhandled token type: %d\n", token->kind);
  }

  // Update the cursor with the current location
  token_cursor->current = token;

  TOKEN_LOG(token_cursor->current, "Next token is %d\n", token_cursor->current->kind);

  return token;
}

// Tokenize the script source for the provided file stream
TokenHeader *flux_script_tokenize(FILE *script_file) {
  if (script_token_buffer == NULL) {
    script_token_buffer = flux_memory_alloc(TOKEN_BUFFER_INITIAL_SIZE);
    TOKEN_LOG(script_token_buffer, "Allocated %d bytes for token buffer\n", TOKEN_BUFFER_INITIAL_SIZE);
  }

  TokenHeader *current_token = script_token_buffer;
  TokenCursor token_cursor;
  token_cursor.current = current_token;

  short c = 0;
  int token_state = TSTATE_NONE;

  char str_buffer[2048]; // TODO: This may need to be adjustable
  char *str_ptr = &str_buffer[0];

  TOKEN_LOG(current_token, "--- TOKENIZATION START ---\n");

  while (!feof(script_file)) {
    c = fgetc(script_file);

    /* printf("Char: %c\n", c); */

    // Are we in a string?
    if (is_state(token_state, TSTATE_STRING)) {
      if (c == '"') {
        // TODO: Is the previous char a backslash?
        // Save the string token
        TokenString *str_token = (TokenString*)current_token;
        str_token->header.kind = TokenKindString;
        str_token->length = str_ptr - &str_buffer[0];
        strcpy(str_token->string, &str_buffer[0]);
        TOKEN_LOG(str_token, "String - %s (length: %d)\n", str_token->string, str_token->length);

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

        current_token = flux_script_token_next(&token_cursor);
        current_token->kind = TokenKindNone;
        token_state = TSTATE_NONE;
      } else {
        // TODO: Check buffer bounds
        *str_ptr = c;
        str_ptr++;
        *str_ptr = '\0';
      }
    } else if (is_state(token_state, TSTATE_SYMBOL)) {
      if (c == '-' || c == '_' || isalnum(c)) {
        *str_ptr = c;
        str_ptr++;
        *str_ptr = '\0';
      } else {
        // End the symbol
        TokenSymbol *symbol_token = (TokenSymbol*)current_token;
        symbol_token->header.kind = TokenKindSymbol;
        symbol_token->length = str_ptr - &str_buffer[0];
        symbol_token->is_quoted = flux_flag_check(token_state, FLAG_QUOTED);
        strcpy(symbol_token->string, &str_buffer[0]);
        TOKEN_LOG(symbol_token, "Symbol - %s (length: %d)\n", symbol_token->string, symbol_token->length);

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

        // Put the character back on the stream to read it again
        ungetc(c, script_file);

        current_token = flux_script_token_next(&token_cursor);
        current_token->kind = TokenKindNone;
        token_state = TSTATE_NONE;
      }
    } else if (is_state(token_state, TSTATE_KEYWORD)) {
      if (c == '-' || c == '_' || isalnum(c)) {
        *str_ptr = c;
        str_ptr++;
        *str_ptr = '\0';
      } else {
        // End the keyword
        TokenKeyword *keyword_token = (TokenKeyword*)current_token;
        keyword_token->header.kind = TokenKindKeyword;
        keyword_token->length = str_ptr - &str_buffer[0];
        strcpy(keyword_token->string, &str_buffer[0]);
        TOKEN_LOG(keyword_token, "Keyword - :%s (length: %d)\n", keyword_token->string, keyword_token->length);

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

        // Put the character back on the stream to read it again
        ungetc(c, script_file);

        current_token = flux_script_token_next(&token_cursor);
        current_token->kind = TokenKindNone;
        token_state = TSTATE_NONE;
      }
    } else if (is_state(token_state, TSTATE_NUMBER)) {
      if (isdigit(c)) {
        *str_ptr = c;
        str_ptr++;
        *str_ptr = '\0';
      } else {
        // End the keyword
        TokenInteger *integer_token = (TokenInteger*)current_token;
        integer_token->header.kind = TokenKindInteger;
        integer_token->number = atoi(str_buffer);
        TOKEN_LOG(integer_token, "Integer - %d\n", integer_token->number);

        // Should the number be negative?
        if (flux_flag_check(token_state, FLAG_MINUS)) {
          integer_token->number = -1 * integer_token->number;
        }

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

        // Put the character back on the stream to read it again
        ungetc(c, script_file);

        current_token = flux_script_token_next(&token_cursor);
        current_token->kind = TokenKindNone;
        token_state = TSTATE_NONE;
      }
    } else {
      if (c == '(') {
        TokenParen *paren = (TokenParen*)current_token;
        paren->header.kind = TokenKindParen;
        paren->is_open = 1;
        TOKEN_LOG(paren, "Open Paren\n");
        current_token = flux_script_token_next(&token_cursor);
      } else if (c == ')') {
        // Are we in a string?
        if (token_state == TSTATE_STRING) {
          continue;
        }

        TokenParen *paren = (TokenParen*)current_token;
        paren->header.kind = TokenKindParen;
        paren->is_open = 0;
        TOKEN_LOG(paren, "Close Paren\n");

        current_token = flux_script_token_next(&token_cursor);
        current_token->kind = TokenKindNone;
        token_state = TSTATE_NONE;
      } else if (c == '"') {
        token_state = TSTATE_STRING;
      } else if (c == ':') {
        token_state = TSTATE_KEYWORD;
      } else if (c == '\'') {
        token_state = TSTATE_SYMBOL | FLAG_QUOTED;
      } else if (isalpha(c)) {
        *str_ptr = c;
        str_ptr++;
        *str_ptr = '\0';
        token_state = TSTATE_SYMBOL;
      } else if (c == '-') {
        token_state = TSTATE_NUMBER | FLAG_MINUS;
      } else if (isdigit(c)) {
        *str_ptr = c;
        str_ptr++;
        *str_ptr = '\0';
        token_state = TSTATE_NUMBER;
      }
    }
  }

  TOKEN_LOG(current_token, "Finished tokenization!\n");

  // Return the location of the token buffer since it holds the first token
  return script_token_buffer;
}

typedef enum {
  PSTATE_NONE,
} ParseState;

void *flux_script_expr_list_cursor_init(ExprList *list, ExprListCursor *list_cursor) {
  list_cursor->index = 0;
  list_cursor->list = list;
  list_cursor->current = NULL;
}

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

ExprHeader *flux_script_expr_list_next(ExprListCursor *list_cursor) {
  if (list_cursor->current == NULL) {
    PARSE_LOG(list_cursor->list, "First iteration inside list, first value at %x\n", &list_cursor->list->items);
    // If there's no current item, send the *pointer* to the location of
    // the first list item so that any expression data is set there
    list_cursor->current = (ExprHeader*)(&(list_cursor->list->items));
    return list_cursor->current;
  }

  ExprHeader *expr = list_cursor->current;
  ExprHeader *sub_expr = NULL;
  ExprListCursor sub_list_cursor;

  switch(expr->kind) {
  case ExprKindList:
    PARSE_LOG(expr, "Item is List\n");

    // Loop over the sub-list to get the position after the list is complete
    flux_script_expr_list_cursor_init(expr, &sub_list_cursor);
    PARSE_LOG(expr, "Sub list length %d\n", sub_list_cursor.list->length);
    do {
      sub_expr = flux_script_expr_list_next(&sub_list_cursor);
      PARSE_LOG(expr, "Sub expr %d\n", sub_list_cursor.index);
    } while (sub_list_cursor.index <= sub_list_cursor.list->length);

    list_cursor->current = sub_list_cursor.current;
    PARSE_LOG(list_cursor->list, "NEXT of List: %x\n", list_cursor->current);
    break;

  case ExprKindSymbol:
    PARSE_LOG(expr, "Item is Symbol\n");
    list_cursor->current = expr + sizeof(ExprSymbol) + ((ExprSymbol*)expr)->length + 1;
    break;

  case ExprKindKeyword:
    PARSE_LOG(expr, "Item is Keyword\n");
    list_cursor->current = expr + sizeof(ExprKeyword) + ((ExprKeyword*)expr)->length + 1;
    break;

  case ExprKindValue:
    PARSE_LOG(expr, "Item is Value\n");
    list_cursor->current = expr + sizeof(ExprHeader) + flux_script_value_size(&(((ExprValue *)expr)->value));
    break;

  default:
    PANIC("Unhandled expr type: %d\n", expr->kind);
  }

  PARSE_LOG(expr, "NEXT item: %x (moved %d)\n", list_cursor->current, list_cursor->current - expr);

  return list_cursor->current;
}

ExprHeader *flux_script_expr_list_push(ExprListCursor *list_cursor) {
  // TODO: This is duplicated with behavior in *_list_next, need to centralize
  if (list_cursor->current == NULL) {
    PARSE_LOG(list_cursor->list, "First push to list, first value at %x\n", &list_cursor->list->items);
    // If there's no current item, send the *pointer* to the location of
    // the first list item so that any expression data is set there
    list_cursor->current = (ExprHeader*)(&(list_cursor->list->items));
  }

  list_cursor->list->length++;
  PARSE_LOG(list_cursor->list, "Push new item on cursor at %x, new length: %d\n", list_cursor->current, list_cursor->list->length);
  return list_cursor->current;
}

ExprHeader *flux_script_expr_list_init(ExprList *list, ExprListCursor *list_cursor) {
  // Initialize the list's memory location
  list->length = 0;
  list->header.kind = ExprKindList;
  list->items[0].kind = ExprKindNone;

  // Initialize the cursor
  flux_script_expr_list_cursor_init(list, list_cursor);
}

ExprList *flux_script_parse_list(TokenCursor *token_cursor, ExprListCursor *list_cursor) {
  PARSE_LOG(list_cursor->list, "Parse list starting at %x...\n", &list_cursor->list->items);

  TokenHeader *token = token_cursor->current;
  while (token->kind != TokenKindNone) {
    PARSE_LOG(list_cursor->list, "Parser got token %d (%x)\n", token->kind, token);

    // Parse sub-lists
    if (token->kind == TokenKindParen) {
      if (((TokenParen*)token)->is_open) {
        PARSE_LOG(list_cursor->list, "START sub-list --\n");

        // Start a new list and its cursor
        ExprList *sub_list = (ExprList*)flux_script_expr_list_push(list_cursor);
        ExprListCursor sub_list_cursor;
        flux_script_expr_list_init(sub_list, &sub_list_cursor);

        // Move to the next token before recursing
        token = flux_script_token_next(token_cursor);

        // TODO: Make this a legitimate parse error
        if (token->kind == TokenKindNone) {
          PANIC("List ended prematurely!");
        }

        PARSE_LOG(list_cursor->list, "Begin sub-list parse starting at %x...\n", list_cursor->current);

        // Parse the list contents recursively.  The sub-cursor will tell us
        // where to resume the parent list's data once this function completes.
        flux_script_parse_list(token_cursor, &sub_list_cursor);

        // Trace where we're headed next
        PARSE_LOG(list_cursor->current, "Outer list will resume at %x\n", sub_list_cursor.current);

        // Set the outer cursor to be where the inner cursor left off
        list_cursor->current = sub_list_cursor.current;
      } else {
        PARSE_LOG(list_cursor->current, "END list, length %d...\n", list_cursor->list->length);
        return list_cursor->list;
      }
    } else if (token->kind == TokenKindSymbol) {
      ExprSymbol *symbol = (ExprSymbol*)flux_script_expr_list_push(list_cursor);
      PARSE_LOG(symbol, "Setting symbol: %s\n", ((TokenSymbol *)token)->string);
      symbol->header.kind = ExprKindSymbol;
      symbol->length = ((TokenSymbol*)token)->length;
      strcpy(symbol->name, ((TokenSymbol*)token)->string);
      symbol->is_quoted = ((TokenSymbol*)token)->is_quoted;

      // Move the cursor forward
      flux_script_expr_list_next(list_cursor);
    } else if (token->kind == TokenKindKeyword) {
      ExprKeyword *keyword = (ExprKeyword*)flux_script_expr_list_push(list_cursor);
      PARSE_LOG(keyword, "Setting keyword: %s\n", ((TokenKeyword *)token)->string);
      keyword->header.kind = ExprKindKeyword;
      keyword->length = ((TokenKeyword*)token)->length;
      strcpy(keyword->name, ((TokenKeyword*)token)->string);

      // Move the cursor forward
      flux_script_expr_list_next(list_cursor);
    } else if (token->kind == TokenKindInteger) {
      ExprValue *integer = (ExprValue*)flux_script_expr_list_push(list_cursor);
      PARSE_LOG(integer, "Setting integer: %d\n", ((TokenInteger *)token)->number);
      integer->header.kind = ExprKindValue;

      // Set the value
      ValueInteger *int_value = (ValueInteger *)&(integer->value);
      int_value->header.kind = ValueKindInteger;
      // TODO: Somehow this is setting the token kind!  WAT
      /* int_value->value = ((TokenInteger *)token)->number; */

      // Move the cursor forward
      flux_script_expr_list_next(list_cursor);
    } else if (token->kind == TokenKindString) {
      ExprValue *string = (ExprValue*)flux_script_expr_list_push(list_cursor);
      PARSE_LOG(string, "Setting string: %d\n", ((TokenString *)token)->string);
      string->header.kind = ExprKindValue;

      // Set the value
      ValueString *str_value = (ValueString*)&(string->value);
      str_value->header.kind = ValueKindString;
      str_value->length = ((TokenString*)token)->length;
      strcpy(str_value->string, ((TokenString*)token)->string);

      // Move the cursor forward
      flux_script_expr_list_next(list_cursor);
    } else if (token->kind != TokenKindNone) {
      // If we see any unusual token kind, bail out
      PANIC("Parser received unexpected TokenKind %d\n", token->kind);
    }

    // Get the next token to process
    token = flux_script_token_next(token_cursor);
  }

  // At this point, assume we've reached TokenKindNone so set the
  // current expression kind to mark the end
  PARSE_LOG(list_cursor->current, "EXIT list parse, set ExprKindNone\n");
}

ExprList *flux_script_parse(TokenHeader *start_token) {
  // This function will be called recursively for all lists so that
  // we can maintain the ExprListIterator on the stack
  //
  // We start parsing by creating an initial list in memory for the
  // top-level expression list, then we recursively walk the tree of
  // lists.
  //
  // The whole tree is written in a linear strip of memory.  Since the
  // high level unit of iteration are lists, we can recursively walk the
  // tree.

  // Allocate the parse buffer if necessary
  if (script_parse_buffer == NULL) {
    script_parse_buffer = flux_memory_alloc(PARSE_BUFFER_INITIAL_SIZE);
    PARSE_LOG(script_parse_buffer, "Allocated %d bytes for parse buffer\n", PARSE_BUFFER_INITIAL_SIZE);
  }

  // Set the token cursor
  TokenCursor token_cursor;
  token_cursor.current = start_token;

  // Create the top-level expression list
  ExprList *top_level_list = (ExprList*)script_parse_buffer;
  ExprListCursor list_cursor;
  flux_script_expr_list_init(top_level_list, &list_cursor);

  PARSE_LOG(list_cursor.list, "--- PARSING START ---\n");

  // Parse the top-level list, this will parse everything recursively
  flux_script_parse_list(&token_cursor, &list_cursor);

  // Set the final item to None to ensure iteration can finish
  list_cursor.current->kind = ExprKindNone;

  PARSE_LOG(list_cursor.current, "FINISHED parsing top-level!\n");

  return top_level_list;
}

unsigned int flux_script_eval(FILE *script_file) {
  TokenHeader *first_token = flux_script_tokenize(script_file);
  if (first_token != NULL) {
    return flux_script_parse(first_token);
  }

  // TODO: Better return type
  return 1;
}

unsigned int flux_script_eval_string(char *script_string) {
  FILE *stream = flux_file_from_string(script_string);
  flux_script_eval(stream);
}
