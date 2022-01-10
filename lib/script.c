#include <stdio.h>
#include <string.h>
#include "flux.h"
#include "flux-internal.h"

void *script_token_buffer = NULL;

#define TOKEN_BUFFER_INITIAL_SIZE 1024

enum ParseState {
  STATE_NONE,
  STATE_SYMBOL,
  STATE_KEYWORD,
  STATE_STRING,
  STATE_NUMBER,
};

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
TokenHeader *flux_script_token_next(TokenHeader *token) {
  switch (token->kind) {
  case TokenKindNone:
    // We're already at the end of the token array
    return token;
  case TokenKindParen:
    return token + sizeof(TokenParen);
  case TokenKindInteger:
    return token + sizeof(TokenInteger);
  case TokenKindFloat:
    return token + sizeof(TokenFloat);
  case TokenKindString:
  case TokenKindSymbol:
  case TokenKindKeyword:
    return token + sizeof(TokenString) + ((TokenString*)token)->length + 1;
  default:
    // TODO: Panic
    printf("%s: Unhandled token type: %d\n", __func__, token->kind);
  }
}

// Tokenize the script source for the provided file stream
TokenHeader *flux_script_tokenize(FILE *script_file) {
  if (script_token_buffer == NULL) {
    script_token_buffer = flux_memory_alloc(TOKEN_BUFFER_INITIAL_SIZE);
  }

  TokenHeader *current_token = script_token_buffer;

  short c = 0;
  int token_state = STATE_NONE;

  char str_buffer[2048]; // TODO: This may need to be adjustable
  char *str_ptr = &str_buffer[0];

  while (c != EOF) {
    c = fgetc(script_file);

    /* printf("Char: %c\n", c); */

    // Are we in a string?
    if (is_state(token_state, STATE_STRING)) {
      if (c == '"') {
        // TODO: Is the previous char a backslash?
        // Save the string token
        TokenString *str_token = (TokenString*)current_token;
        str_token->header.kind = TokenKindString;
        str_token->length = str_ptr - &str_buffer[0];
        strcpy(str_token->string, &str_buffer[0]);
        current_token = flux_script_token_next(current_token);

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

        token_state = STATE_NONE;
      } else {
        // TODO: Check buffer bounds
        *str_ptr = c;
        str_ptr++;
        *str_ptr = '\0';
      }
    } else if (is_state(token_state, STATE_SYMBOL)) {
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
        current_token = flux_script_token_next(current_token);

        // Is the symbol quoted?
        symbol_token->is_quoted = flux_flag_check(token_state, FLAG_QUOTED);

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

        // Seek one character back in the stream
        fseek(script_file, -1, SEEK_CUR);

        token_state = STATE_NONE;
      }
    } else if (is_state(token_state, STATE_KEYWORD)) {
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
        current_token = flux_script_token_next(current_token);

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

        // Seek one character back in the stream
        fseek(script_file, -1, SEEK_CUR);

        token_state = STATE_NONE;
      }
    } else if (is_state(token_state, STATE_NUMBER)) {
      if (isdigit(c)) {
        *str_ptr = c;
        str_ptr++;
        *str_ptr = '\0';
      } else {
        // End the keyword
        TokenInteger *integer_token = (TokenInteger*)current_token;
        integer_token->header.kind = TokenKindInteger;
        integer_token->number = atoi(str_buffer);
        current_token = flux_script_token_next(current_token);

        // Should the number be negative?
        if (flux_flag_check(token_state, FLAG_MINUS)) {
          integer_token->number = -1 * integer_token->number;
        }

        // Reset the string buffer
        str_ptr = &str_buffer[0];
        *str_ptr = '\0';

        // Seek one character back in the stream
        fseek(script_file, -1, SEEK_CUR);

        token_state = STATE_NONE;
      }
    } else {
      if (c == '(') {
        TokenParen *paren = (TokenParen*)current_token;
        paren->header.kind = TokenKindParen;
        paren->is_open = 1;
        current_token = flux_script_token_next(current_token);
      } else if (c == ')') {
        // Are we in a string?
        if (token_state == STATE_STRING) {
          continue;
        }

        TokenParen *paren = (TokenParen*)current_token;
        paren->header.kind = TokenKindParen;
        paren->is_open = 0;
        current_token = flux_script_token_next(current_token);
      } else if (c == '"') {
        token_state = STATE_STRING;
      } else if (c == ':') {
        token_state = STATE_KEYWORD;
      } else if (c == '\'') {
        token_state = STATE_SYMBOL | FLAG_QUOTED;
      } else if (isalpha(c)) {
        *str_ptr = c;
        str_ptr++;
        *str_ptr = '\0';
        token_state = STATE_SYMBOL;
      } else if (c == '-') {
        token_state = STATE_NUMBER | FLAG_MINUS;
      } else if (isdigit(c)) {
        *str_ptr = c;
        str_ptr++;
        *str_ptr = '\0';
        token_state = STATE_NUMBER;
      }
    }
  }

  // Set the token at the current location to be None to end the sequence
  current_token->kind = TokenKindNone;

  // Return the location of the token buffer since it holds the first token
  return script_token_buffer;
}

// Parse the token list starting with the first token
unsigned int flux_script_parse(TokenHeader *start_token) {
  // STEPS:
  // Loop through the token buffer
  return 0;
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
