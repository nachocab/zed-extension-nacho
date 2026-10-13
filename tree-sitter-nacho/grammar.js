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

// Characters that may sit between the words of a single `text` token. Digits,
// operators and the openers of every highlighted construct are left out so they
// start a token of their own. So is whitespace: a run of prose spanning a space
// would swallow the scheme of a URL after it.
const TEXT_SEPARATOR = String.raw`[,;:?¿¡'’.@~$»”…\\]`;
// A word keeps any `.`-joined parts glued to it, so the `.5` in `x.5` or
// `v1.2` reads as part of the word rather than as a number.
const WORD = String.raw`[\p{L}_][\p{L}\p{N}_]*(\.[\p{L}\p{N}_]+)*`;
// A URL stops at whitespace and at the openers and closers the scanner pairs
// up, so a bracket, string or highlight around it still finds its partner. The
// exception is a balanced `(…)`, as in Wikipedia links, which the scanner pairs
// the same way. Trailing punctuation belongs to the sentence.
const URL_CHAR = String.raw`[^\s()\[\]{}<>"“”«»*\x60]`;
const URL_END = String.raw`[^\s()\[\]{}<>"“”«»*\x60.,;:!?'’]`;
const URL_PARENS = String.raw`\(${URL_CHAR}*\)`;

// A bracket pair holding `content`, one choice per bracket type. The scanner
// only opens one whose partner is on the same line.
const brackets = ($, content) =>
  choice(
    seq(alias($._paren_open, "("), repeat(content), ")"),
    seq(alias($._square_open, "["), repeat(content), "]"),
    seq(alias($._brace_open, "{"), repeat(content), "}"),
  );

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
    $._paren_open,
    $._square_open,
    $._brace_open,
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
    heading_content: ($) => repeat1(choice(alias($._heading_group, $.group), $._heading_text)),
    _heading_group: ($) => brackets($, choice(alias($._heading_group, $.group), $._heading_text)),
    _heading_text: ($) => choice(/[^\s()\[\]{}]([^\n()\[\]{}]*[^\s()\[\]{}])?/, $._stray_bracket),

    _block: ($) => choice($._line, $.blockquote, $._blank_line),

    _blank_line: (_) => /\r?\n/,

    // Lines starting with `>` take the whole line, with nothing highlighted inside.
    blockquote: ($) => seq(token(prec(1, />[^\n]*/)), $._eol),

    _line: ($) =>
      choice(seq($.list_marker, optional($._line_body), $._eol), seq($._line_body, $._eol)),
    // A line comment always runs to the end of its line.
    _line_body: ($) => choice(seq(repeat1($._inline), optional($.comment)), $.comment),

    // A bullet at the start of a line.
    list_marker: (_) => /[-*+•][ \t]/,

    _inline: ($) =>
      choice(
        $.html_comment,
        $.url,
        $.group,
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
        repeat(
          choice($.comment_highlight, $.url, alias($._comment_group, $.group), $._comment_text),
        ),
      ),
    _comment_group: ($) =>
      brackets(
        $,
        choice($.comment_highlight, $.url, alias($._comment_group, $.group), $._comment_text),
      ),
    comment_highlight: ($) =>
      choice(
        seq(alias($._emphasis_open, "*"), $.highlight_content, "*"),
        seq(alias($._code_open, "`"), alias($._code_content, $.highlight_content), "`"),
        seq(alias($._quote_open, '"'), alias($._quote_content, $.highlight_content), '"'),
      ),
    // A delimiter with no partner on the line is plain comment text. Text runs
    // word by word and stops at `/`, so a URL can start after any space and
    // always outlasts the word that starts with its scheme.
    _comment_text: ($) => choice(/[^*`"()\[\]{}\s\/]+/, "/", "*", "`", '"', $._stray_bracket),

    group: ($) => brackets($, $._inline),
    // A bracket with no partner on the line. Inside a pair, the pair's own
    // closer outranks it.
    _stray_bracket: (_) => token(prec(-1, /[()\[\]{}]/)),

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

    // What Zed opens on a modifier-click: a scheme, `://` and the rest of the
    // word.
    url: (_) =>
      token(
        new RegExp(
          String.raw`[a-zA-Z][a-zA-Z0-9+.-]*://(${URL_CHAR}|${URL_PARENS})*(${URL_END}|${URL_PARENS})`,
        ),
      ),

    _other: (_) => token(prec(-1, /[^ \t\r\n]/)),
  },
});
