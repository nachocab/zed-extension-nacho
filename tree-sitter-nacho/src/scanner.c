#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>

enum TokenType {
  EOL,
  EMPHASIS_OPEN,
  CODE_OPEN,
  QUOTE_OPEN,
  STRING_OPEN,
  CURLY_STRING_OPEN,
  GUILLEMET_STRING_OPEN,
  PAREN_OPEN,
  SQUARE_OPEN,
  BRACE_OPEN,
  ERROR_SENTINEL,
};

// Where a bracket sits decides what its content can hold, and so which spans
// can hide its partner. Heading text holds nothing but nested brackets.
typedef enum { BODY, COMMENT, HEADING } Context;

// How far an opener looks for its closer, a highlight for its closing
// delimiter, or a string for its closing quote. Every scan is linear in this
// window, which bounds what a pathological line (say, minified JSON) can cost.
#define MAX_LOOKAHEAD 1024

#define NONE UINT16_MAX

enum Bracket { PAREN, SQUARE, BRACE, BRACKET_COUNT };

// Scratch space for matching brackets. `chars` holds the rest of the line, and
// each `next_*` array the index of the first delimiter at or after a position.
typedef struct {
  int32_t chars[MAX_LOOKAHEAD];
  uint16_t length;
  uint16_t next_star[MAX_LOOKAHEAD + 1];
  uint16_t next_tick[MAX_LOOKAHEAD + 1];
  uint16_t next_quote[MAX_LOOKAHEAD + 1];
  uint16_t next_string_close[3][MAX_LOOKAHEAD + 1];
  uint16_t next_html_close[MAX_LOOKAHEAD + 1];
  uint16_t brackets[MAX_LOOKAHEAD];
  uint16_t closers[BRACKET_COUNT][MAX_LOOKAHEAD];
} Scanner;

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
  for (unsigned n = 0; !at_line_end(lexer) && n < MAX_LOOKAHEAD; n++) {
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
  for (unsigned n = 0; !at_line_end(lexer) && n < MAX_LOOKAHEAD; n++) {
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

static int bracket_opened_by(int32_t c) {
  switch (c) {
  case '(':
    return PAREN;
  case '[':
    return SQUARE;
  case '{':
    return BRACE;
  default:
    return -1;
  }
}

static int bracket_closed_by(int32_t c) {
  switch (c) {
  case ')':
    return PAREN;
  case ']':
    return SQUARE;
  case '}':
    return BRACE;
  default:
    return -1;
  }
}

static const int32_t STRING_CLOSERS[3] = {'"', 0x201D, 0x00BB}; // " ” »

static int string_opened_by(int32_t c) {
  switch (c) {
  case '"':
    return 0;
  case 0x201C: // “
    return 1;
  case 0x00AB: // «
    return 2;
  default:
    return -1;
  }
}

static bool starts_with(const Scanner *s, uint16_t i, const char *prefix) {
  for (; *prefix; prefix++, i++) {
    if (i >= s->length || s->chars[i] != *prefix) {
      return false;
    }
  }
  return true;
}

static bool is_comment_marker(const Scanner *s, uint16_t i) {
  return starts_with(s, i, "# ") || starts_with(s, i, "// ") ||
         starts_with(s, i, "-- ");
}

// The index just past an HTML comment terminator (`--\s*>`) starting at `i`,
// or NONE.
static uint16_t html_close_at(const Scanner *s, uint16_t i) {
  if (!starts_with(s, i, "--")) {
    return NONE;
  }
  uint16_t k = i + 2;
  while (k < s->length && (s->chars[k] == ' ' || s->chars[k] == '\t')) {
    k++;
  }
  return k < s->length && s->chars[k] == '>' ? k + 1 : NONE;
}

// Fills the `next_*` arrays from right to left. A string's closer is the first
// unescaped one, so a backslash defers to the position after the character it
// escapes.
static void index_delimiters(Scanner *s) {
  uint16_t n = s->length;
  s->next_star[n] = s->next_tick[n] = s->next_quote[n] = NONE;
  s->next_html_close[n] = NONE;
  for (int k = 0; k < 3; k++) {
    s->next_string_close[k][n] = NONE;
  }
  for (int i = n - 1; i >= 0; i--) {
    int32_t c = s->chars[i];
    s->next_star[i] = c == '*' ? i : s->next_star[i + 1];
    s->next_tick[i] = c == '`' ? i : s->next_tick[i + 1];
    s->next_quote[i] = c == '"' ? i : s->next_quote[i + 1];
    uint16_t html_close = html_close_at(s, i);
    s->next_html_close[i] =
        html_close != NONE ? html_close : s->next_html_close[i + 1];
    for (int k = 0; k < 3; k++) {
      uint16_t *next = s->next_string_close[k];
      if (c == '\\') {
        next[i] = i + 2 <= n ? next[i + 2] : NONE;
      } else {
        next[i] = c == STRING_CLOSERS[k] ? i : next[i + 1];
      }
    }
  }
}

// The index just past a span the grammar lexes whole (a highlight, a string,
// an HTML comment) opening at `i`, 0 if none opens there, or NONE if the rest
// of the line belongs to it.
static uint16_t span_end(const Scanner *s, uint16_t i, Context context) {
  int32_t c = s->chars[i];
  bool highlight = c == '*' || c == '`' || (c == '"' && context == COMMENT);
  if (highlight) {
    if (i + 1 >= s->length || s->chars[i + 1] == ' ' ||
        s->chars[i + 1] == c) {
      return 0;
    }
    const uint16_t *next = c == '*'   ? s->next_star
                           : c == '`' ? s->next_tick
                                      : s->next_quote;
    return next[i + 1] == NONE ? 0 : next[i + 1] + 1;
  }
  if (context != BODY) {
    return 0;
  }
  int string = string_opened_by(c);
  if (string >= 0) {
    uint16_t close = s->next_string_close[string][i + 1];
    return close == NONE ? 0 : close + 1;
  }
  if (is_comment_marker(s, i)) {
    return NONE;
  }
  if (starts_with(s, i, "<!--")) {
    return s->next_html_close[i + 4 > s->length ? s->length : i + 4];
  }
  return 0;
}

// Whether the opener at the start of the buffer has a partner, judged the way
// the grammar will lex the line. Walking left to right, highlights, strings and
// HTML comments are skipped whole, and in the body a comment marker ends the
// line, collecting the brackets in between. Then, right to left, each opener
// takes the nearest closer of its type that no pair to its right has already
// enclosed. An opener without one stays text, transparent to the pairs around
// it.
static bool has_partner(Scanner *s, Context context) {
  if (context != HEADING) {
    index_delimiters(s);
  }
  uint16_t bracket_count = 0;
  for (uint16_t i = 0; i < s->length;) {
    if (bracket_opened_by(s->chars[i]) >= 0 ||
        bracket_closed_by(s->chars[i]) >= 0) {
      s->brackets[bracket_count++] = i++;
      continue;
    }
    uint16_t end = context == HEADING ? 0 : span_end(s, i, context);
    if (end == NONE) {
      break;
    }
    i = end ? end : i + 1;
  }

  uint16_t depth[BRACKET_COUNT] = {0};
  for (int b = bracket_count - 1; b >= 0; b--) {
    uint16_t i = s->brackets[b];
    int closed = bracket_closed_by(s->chars[i]);
    if (closed >= 0) {
      s->closers[closed][depth[closed]++] = i;
      continue;
    }
    int opened = bracket_opened_by(s->chars[i]);
    if (!depth[opened]) {
      continue;
    }
    if (i == 0) {
      return true;
    }
    uint16_t partner = s->closers[opened][--depth[opened]];
    for (int other = 0; other < BRACKET_COUNT; other++) {
      while (depth[other] && s->closers[other][depth[other] - 1] < partner) {
        depth[other]--;
      }
    }
  }
  return false;
}

// Opens a bracket pair when its partner is on the same line, reading the rest
// of the line as lookahead.
static bool scan_bracket_open(Scanner *s, TSLexer *lexer, Context context,
                              enum TokenType symbol) {
  s->length = 0;
  while (!at_line_end(lexer) && s->length < MAX_LOOKAHEAD) {
    s->chars[s->length++] = lexer->lookahead;
    lexer->advance(lexer, false);
    if (s->length == 1) {
      lexer->mark_end(lexer);
    }
  }
  if (!has_partner(s, context)) {
    return false;
  }
  lexer->result_symbol = symbol;
  return true;
}

void *tree_sitter_nacho_external_scanner_create(void) {
  return ts_calloc(1, sizeof(Scanner));
}

void tree_sitter_nacho_external_scanner_destroy(void *payload) {
  ts_free(payload);
}

// The buffers only hold scratch state for a single scan.
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

  // Only body text can open a string, and only a comment can open a quote.
  Context context = valid_symbols[STRING_OPEN]  ? BODY
                    : valid_symbols[QUOTE_OPEN] ? COMMENT
                                                : HEADING;

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
  case '(':
    return valid_symbols[PAREN_OPEN] &&
           scan_bracket_open(payload, lexer, context, PAREN_OPEN);
  case '[':
    return valid_symbols[SQUARE_OPEN] &&
           scan_bracket_open(payload, lexer, context, SQUARE_OPEN);
  case '{':
    return valid_symbols[BRACE_OPEN] &&
           scan_bracket_open(payload, lexer, context, BRACE_OPEN);
  default:
    return false;
  }
}
