#include "tree_sitter/parser.h"

#include <stdbool.h>

enum TokenType {
  EOL,
  EMPHASIS_OPEN,
  CODE_OPEN,
  QUOTE_OPEN,
  STRING_OPEN,
  CURLY_STRING_OPEN,
  GUILLEMET_STRING_OPEN,
  ERROR_SENTINEL,
};

static inline bool at_line_end(TSLexer *lexer) {
  return lexer->eof(lexer) || lexer->lookahead == '\n' ||
         lexer->lookahead == '\r';
}

// Opens a highlight such as `*text*` when its closing delimiter is further
// along the same line and the opener isn't followed by a space. Only the opener
// becomes the token; the rest of the line is lookahead.
static bool scan_highlight_open(TSLexer *lexer, int32_t delimiter,
                                enum TokenType symbol) {
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  if (lexer->lookahead == ' ' || lexer->lookahead == delimiter) {
    return false;
  }
  while (!at_line_end(lexer)) {
    if (lexer->lookahead == delimiter) {
      lexer->result_symbol = symbol;
      return true;
    }
    lexer->advance(lexer, false);
  }
  return false;
}

// Opens a string when it closes on the same line. A backslash escapes the
// character after it, so `\"` doesn't close a `"` string.
static bool scan_string_open(TSLexer *lexer, int32_t closer,
                             enum TokenType symbol) {
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  while (!at_line_end(lexer)) {
    if (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (at_line_end(lexer)) {
        return false;
      }
    } else if (lexer->lookahead == closer) {
      lexer->result_symbol = symbol;
      return true;
    }
    lexer->advance(lexer, false);
  }
  return false;
}

void *tree_sitter_nacho_external_scanner_create(void) { return NULL; }

void tree_sitter_nacho_external_scanner_destroy(void *payload) {}

unsigned tree_sitter_nacho_external_scanner_serialize(void *payload,
                                                      char *buffer) {
  return 0;
}

void tree_sitter_nacho_external_scanner_deserialize(void *payload,
                                                    const char *buffer,
                                                    unsigned length) {}

bool tree_sitter_nacho_external_scanner_scan(void *payload, TSLexer *lexer,
                                             const bool *valid_symbols) {
  // Every symbol is valid during error recovery, where a zero-width EOL at the
  // end of the file would never advance.
  if (valid_symbols[ERROR_SENTINEL]) {
    return false;
  }

  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    lexer->advance(lexer, true);
  }

  if (valid_symbols[EOL] && at_line_end(lexer)) {
    if (lexer->lookahead == '\r') {
      lexer->advance(lexer, false);
    }
    if (lexer->lookahead == '\n') {
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);
    lexer->result_symbol = EOL;
    return true;
  }

  switch (lexer->lookahead) {
  case '*':
    return valid_symbols[EMPHASIS_OPEN] &&
           scan_highlight_open(lexer, '*', EMPHASIS_OPEN);
  case '`':
    return valid_symbols[CODE_OPEN] &&
           scan_highlight_open(lexer, '`', CODE_OPEN);
  case '"':
    if (valid_symbols[QUOTE_OPEN]) {
      return scan_highlight_open(lexer, '"', QUOTE_OPEN);
    }
    return valid_symbols[STRING_OPEN] &&
           scan_string_open(lexer, '"', STRING_OPEN);
  case 0x201C: // “
    return valid_symbols[CURLY_STRING_OPEN] &&
           scan_string_open(lexer, 0x201D, CURLY_STRING_OPEN); // ”
  case 0x00AB: // «
    return valid_symbols[GUILLEMET_STRING_OPEN] &&
           scan_string_open(lexer, 0x00BB, GUILLEMET_STRING_OPEN); // »
  default:
    return false;
  }
}
