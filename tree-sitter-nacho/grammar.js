/**
 * @file Nacho, a variation of Markdown for note-taking
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

// Heading levels with their own section node. `#{8,}` all share level 8.
const LEVELS = [2, 3, 4, 5, 6, 7, 8];

// Every section deeper than `level`, all exposed as one `section` node type.
const sections = ($, level) =>
  LEVELS.filter((n) => n > level).map((n) => alias($[`_section_${n}`], $.section));

// Characters that may sit between the words of a single `text` token. Grouping
// a run of prose into one token keeps the tree small; digits, operators and the
// openers of every highlighted construct are left out so they start a token of
// their own.
const TEXT_SEPARATOR = String.raw`[ \t,;:()?¿¡'’\[\]{}.@~$»”…\\]`;
const WORD = String.raw`[\p{L}_][\p{L}\p{N}_]*`;

module.exports = grammar({
  name: "nacho",

  // `_eol` ends a non-blank line, either on a newline or at the end of the
  // file, which lets the last line go without a trailing newline.
  externals: ($) => [
    $._eol,
    $._emphasis_open,
    $._code_open,
    $._quote_open,
    $._string_open,
    $._curly_string_open,
    $._guillemet_string_open,
    $._error_sentinel,
  ],

  extras: (_) => [/[ \t]/],

  rules: {
    source_file: ($) => repeat(choice($._block, ...sections($, 1))),

    // A section runs until the next heading of the same or a shallower level,
    // so deeper sections nest inside it. Zed reads the outline hierarchy off
    // this nesting. Right associativity keeps each line in the innermost open
    // section.
    ...Object.fromEntries(
      LEVELS.map((n) => [
        `_section_${n}`,
        ($) =>
          prec.right(seq($[`heading_${n}`], $._eol, repeat(choice($._block, ...sections($, n))))),
      ]),
    ),

    ...Object.fromEntries(
      LEVELS.map((n) => [
        `heading_${n}`,
        ($) =>
          seq(
            field("marker", alias($[`_heading_marker_${n}`], $.heading_marker)),
            optional(field("content", $.heading_content)),
          ),
      ]),
    ),
    ...Object.fromEntries(
      LEVELS.map((n) => [`_heading_marker_${n}`, (_) => (n === 8 ? /#{8}#*/ : "#".repeat(n))]),
    ),
    heading_content: (_) => /[^\s]([^\n]*[^\s])?/,

    _block: ($) => choice($._line, $.blockquote, $._blank_line),

    _blank_line: (_) => /\r?\n/,

    // Lines starting with `>` take the whole line, with nothing highlighted inside.
    blockquote: ($) => seq(token(prec(1, />[^\n]*/)), $._eol),

    // A line comment always runs to the end of its line.
    _line: ($) => seq(choice(seq(repeat1($._inline), optional($.comment)), $.comment), $._eol),

    _inline: ($) =>
      choice(
        $.html_comment,
        $.emphasis,
        $.code,
        $.string,
        $.number,
        $.operator,
        $._text,
        $._other,
      ),

    // `(#|//|--) ` starts a comment anywhere in a line.
    comment: ($) =>
      seq(
        alias(/(#|\/\/|--) /, $.comment_marker),
        repeat(choice($.comment_highlight, $._comment_text)),
      ),
    comment_highlight: ($) =>
      choice(
        seq(alias($._emphasis_open, "*"), $.highlight_content, "*"),
        seq(alias($._code_open, "`"), alias($._code_content, $.highlight_content), "`"),
        seq(alias($._quote_open, '"'), alias($._quote_content, $.highlight_content), '"'),
      ),
    // A delimiter with no partner on the line is plain comment text.
    _comment_text: (_) => choice(/[^*`"\n]+/, "*", "`", '"'),

    // The scanner only opens `emphasis` and `code` (and `comment_highlight`)
    // when the closing delimiter is on the same line, and never before a space.
    emphasis: ($) => seq(alias($._emphasis_open, "*"), $.highlight_content, "*"),
    code: ($) => seq(alias($._code_open, "`"), alias($._code_content, $.highlight_content), "`"),
    highlight_content: (_) => /[^*\n]+/,
    _code_content: (_) => /[^`\n]+/,
    _quote_content: (_) => /[^"\n]+/,

    // Strings close on the line they open on, so a stray quote can't swallow
    // the headings below it.
    string: ($) =>
      choice(
        seq(alias($._string_open, '"'), repeat(choice(/[^"\\\n]+/, $.escape_sequence)), '"'),
        seq(alias($._curly_string_open, "“"), repeat(choice(/[^”\\\n]+/, $.escape_sequence)), "”"),
        seq(
          alias($._guillemet_string_open, "«"),
          repeat(choice(/[^»\\\n]+/, $.escape_sequence)),
          "»",
        ),
      ),
    escape_sequence: (_) => /\\[^\n]/,

    // Spans lines. The body excludes `--\s*>` so the comment closes on the first
    // terminator rather than the last one in the file.
    html_comment: (_) =>
      token(
        seq(
          "<!--",
          repeat(
            choice(
              /[^-]/,
              /(-+[ \t\r\n]+)*-[ \t\r\n]*[^-\s]/,
              /(-+[ \t\r\n]+)*--+[ \t\r\n]*[^->\s]/,
            ),
          ),
          /(-+[ \t\r\n]+)*--+[ \t\r\n]*>/,
        ),
      ),

    // Carries no precedence, which would beat a longer match: `2nd` has to lex
    // as one text run. Where the two tie on length (`10`, `10L`, `1e5`), the
    // number wins by coming first in the grammar.
    number: (_) =>
      token(
        choice(
          /0[xX][0-9a-fA-F]*/,
          /([0-9]+(\.[0-9]+)?|\.[0-9]+)([eE][+-]?[0-9]+)?(UL|ul|ULL|ull|LL|ll|L|l|U|u|F|f)?/,
        ),
      ),

    operator: (_) =>
      /<<-|<-|->>|->|==|!=|<>|<=|>=|%\/%|%%|%\*%|%in%|%o%|%x%|&&|\|\||[-+*\/^=<>!&|]/,

    // Prose. A run that starts with a digit (`2nd`, `3D`) is text too, matching
    // the word boundary a number needs.
    _text: (_) =>
      token(
        choice(
          new RegExp(`${WORD}(${TEXT_SEPARATOR}+${WORD})*`),
          /[0-9][\p{L}\p{N}_]*/,
        ),
      ),

    _other: (_) => token(prec(-1, /[^ \t\r\n]/)),
  },
});
