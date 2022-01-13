#include <stdio.h>
#include <string.h>
#include "flux.h"
#include "flux-internal.h"

void *script_token_buffer = NULL;
void *script_parse_buffer = NULL;

#define TOKEN_BUFFER_INITIAL_SIZE 1024
#define PARSE_BUFFER_INITIAL_SIZE 1024

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
  switch (token_cursor->current->kind) {
  case TokenKindNone:
    // We're already at the end of the token array
    // TODO: Signal this somehow?
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
  case TokenKindSymbol:
  case TokenKindKeyword:
    token += sizeof(TokenString) + ((TokenString*)token)->length + 1;
    break;
  default:
    PANIC("Unhandled token type: %d\n", token->kind);
  }

  // Update the cursor with the current location
  token_cursor->current = token;

  return token;
}

// Tokenize the script source for the provided file stream
TokenHeader *flux_script_tokenize(FILE *script_file) {
  if (script_token_buffer == NULL) {
    script_token_buffer = flux_memory_alloc(TOKEN_BUFFER_INITIAL_SIZE);
  }

  TokenHeader *current_token = script_token_buffer;
  TokenCursor token_cursor;
  token_cursor.current = current_token;

  short c = 0;
  int token_state = TSTATE_NONE;

  char str_buffer[2048]; // TODO: This may need to be adjustable
  char *str_ptr = &str_buffer[0];

  while (c != EOF) {
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
        current_token = flux_script_token_next(&token_cursor);

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

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
        strcpy(symbol_token->string, &str_buffer[0]);
        current_token = flux_script_token_next(&token_cursor);

        // Is the symbol quoted?
        symbol_token->is_quoted = flux_flag_check(token_state, FLAG_QUOTED);

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

        // Seek one character back in the stream
        fseek(script_file, -1, SEEK_CUR);

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
        current_token = flux_script_token_next(&token_cursor);

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

        // Seek one character back in the stream
        fseek(script_file, -1, SEEK_CUR);

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
        current_token = flux_script_token_next(&token_cursor);

        // Should the number be negative?
        if (flux_flag_check(token_state, FLAG_MINUS)) {
          integer_token->number = -1 * integer_token->number;
        }

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

        // Seek one character back in the stream
        fseek(script_file, -1, SEEK_CUR);

        token_state = TSTATE_NONE;
      }
    } else {
      if (c == '(') {
        TokenParen *paren = (TokenParen*)current_token;
        paren->header.kind = TokenKindParen;
        paren->is_open = 1;
        current_token = flux_script_token_next(&token_cursor);
      } else if (c == ')') {
        // Are we in a string?
        if (token_state == TSTATE_STRING) {
          continue;
        }

        TokenParen *paren = (TokenParen*)current_token;
        paren->header.kind = TokenKindParen;
        paren->is_open = 0;
        current_token = flux_script_token_next(&token_cursor);
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

  // Set the token at the current location to be None to end the sequence
  current_token->kind = TokenKindNone;

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
    flux_log_mem(list_cursor->list, "First iteration inside list, first value at %x\n", &list_cursor->list->items);
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
    flux_log_mem(expr, "Item is List\n");

    // Loop over the sub-list to get the position after the list is complete
    flux_script_expr_list_cursor_init(expr, &sub_list_cursor);
    flux_log_mem(expr, "Sub list length %d\n", sub_list_cursor.list->length);
    do {
      sub_expr = flux_script_expr_list_next(&sub_list_cursor);
      flux_log_mem(expr, "Sub expr %d\n", sub_list_cursor.index);
    } while (sub_list_cursor.index <= sub_list_cursor.list->length);

    list_cursor->current = sub_list_cursor.current;
    flux_log_mem(list_cursor->list, "NEXT of List: %x\n", list_cursor->current);
    break;

  case ExprKindSymbol:
    flux_log_mem(expr, "Item is Symbol\n");
    list_cursor->current = expr + sizeof(ExprSymbol) + ((ExprSymbol*)expr)->length + 1;
    break;

  case ExprKindKeyword:
    flux_log_mem(expr, "Item is Keyword\n");
    list_cursor->current = expr + sizeof(ExprKeyword) + ((ExprKeyword*)expr)->length + 1;
    break;

  case ExprKindValue:
    flux_log_mem(expr, "Item is Value\n");
    list_cursor->current = expr + sizeof(ExprHeader) + flux_script_value_size(&(((ExprValue *)expr)->value));
    break;

  default:
    PANIC("Unhandled expr type: %d\n", expr->kind);
  }

  flux_log_mem(expr, "NEXT item: %x (moved %d)\n", list_cursor->current, list_cursor->current - expr);

  return list_cursor->current;
}

ExprHeader *flux_script_expr_list_push(ExprListCursor *list_cursor) {
  // TODO: This is duplicated with behavior in *_list_next, need to centralize
  if (list_cursor->current == NULL) {
    flux_log_mem(list_cursor->list, "First push to list, first value at %x\n", &list_cursor->list->items);
    // If there's no current item, send the *pointer* to the location of
    // the first list item so that any expression data is set there
    list_cursor->current = (ExprHeader*)(&(list_cursor->list->items));
  }

  list_cursor->list->length++;
  flux_log_mem(list_cursor->list, "Push new item on cursor at %x, new length: %d\n", list_cursor->current, list_cursor->list->length);
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
  flux_log_mem(list_cursor->list, "Parse list starting at %x...\n", &list_cursor->list->items);

  TokenHeader *token = token_cursor->current;
  while (token->kind != TokenKindNone) {
    // Parse sub-lists
    if (token->kind == TokenKindParen) {
      if (((TokenParen*)token)->is_open) {
        flux_log_mem(list_cursor->list, "START sub-list --\n");

        // Start a new list and its cursor
        ExprList *sub_list = (ExprList*)flux_script_expr_list_push(list_cursor);
        ExprListCursor sub_list_cursor;
        flux_script_expr_list_init(sub_list, &sub_list_cursor);

        // Move to the next token before recursing
        token = flux_script_token_next(token_cursor);

        flux_log_mem(list_cursor->list, "Begin sub-list parse starting at %x...\n", list_cursor->current);

        flux_script_parse_list(token_cursor, &sub_list_cursor);

        // Set the outer cursor to be where the inner cursor left off
        // TODO: Check this!
        list_cursor->current = sub_list_cursor.current;
      } else {
        // TODO: Check this!
        flux_log_mem(list_cursor->current, "END list, length %d...\n", list_cursor->list->length);
        return list_cursor->list;
      }
    } else if (token->kind == TokenKindSymbol) {
      ExprSymbol *symbol = (ExprSymbol*)flux_script_expr_list_push(list_cursor);
      flux_log_mem(symbol, "Setting symbol: %s\n", ((TokenSymbol *)token)->string);
      symbol->header.kind = ExprKindSymbol;
      symbol->length = ((TokenSymbol*)token)->length;
      strcpy(symbol->name, ((TokenSymbol*)token)->string);
      symbol->is_quoted = ((TokenSymbol*)token)->is_quoted;

      // Move the cursor forward
      flux_script_expr_list_next(list_cursor);
    } else if (token->kind == TokenKindKeyword) {
      ExprKeyword *keyword = (ExprKeyword*)flux_script_expr_list_push(list_cursor);
      flux_log_mem(keyword, "Setting keyword: %s\n", ((TokenKeyword *)token)->string);
      keyword->header.kind = ExprKindKeyword;
      keyword->length = ((TokenKeyword*)token)->length;
      strcpy(keyword->name, ((TokenKeyword*)token)->string);

      // Move the cursor forward
      flux_script_expr_list_next(list_cursor);
    } else if (token->kind == TokenKindInteger) {
      ExprValue *integer = (ExprValue*)flux_script_expr_list_push(list_cursor);
      flux_log_mem(integer, "Setting integer: %d\n", ((TokenInteger *)token)->number);
      integer->header.kind = ExprKindValue;

      // Set the value
      ValueInteger *int_value = (ValueInteger *)&(integer->value);
      int_value->header.kind = ValueKindInteger;
      int_value->value = ((TokenInteger *)token)->number;

      // Move the cursor forward
      flux_script_expr_list_next(list_cursor);
    } else if (token->kind == TokenKindString) {
      ExprValue *string = (ExprValue*)flux_script_expr_list_push(list_cursor);
      flux_log_mem(string, "Setting string: %d\n", ((TokenString *)token)->string);
      string->header.kind = ExprKindValue;

      // Set the value
      ValueString *str_value = (ValueString*)&(string->value);
      str_value->header.kind = ValueKindString;
      str_value->length = ((TokenString*)token)->length;
      strcpy(str_value->string, ((TokenString*)token)->string);

      // Move the cursor forward
      flux_script_expr_list_next(list_cursor);
    }

    // Get the next token to process
    token = flux_script_token_next(token_cursor);
  }

  // At this point, assume we've reached TokenKindNone so set the
  // current expression kind to mark the end
  flux_log_mem(list_cursor->current, "EXIT list parse, set ExprKindNone\n");
}

// Parse the token list starting with the first token
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
  }

  // Set the token cursor
  TokenCursor token_cursor;
  token_cursor.current = start_token;

  // Create the top-level expression list
  ExprList *top_level_list = (ExprList*)script_parse_buffer;
  ExprListCursor list_cursor;
  flux_script_expr_list_init(top_level_list, &list_cursor);

  flux_log_mem(top_level_list, "Top-level list is ready\n");

  // Parse the top-level list, this will parse everything recursively
  flux_script_parse_list(&token_cursor, &list_cursor);

  // Set the final item to None to ensure iteration can finish
  list_cursor.current->kind = ExprKindNone;

  flux_log_mem(list_cursor.current, "FINISHED parsing top-level!\n");

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
