# Nacho for Zed

Nacho is a lightweight variation of Markdown designed for note-taking. This extension brings the [VS Code extension](https://github.com/nachocab/vscode-nacho-language)'s highlighting, outline, and folding to [Zed](https://zed.dev). Nacho2, a port of the VS Code color theme, lives in [nachocab/dotfiles](https://github.com/nachocab/dotfiles/tree/main/zed/themes).

## Syntax

- **Indented headings**: `##` to `########` (or more) start a heading at any indent. Each heading nests under the nearest shallower heading above it, and every level has its own color.
- **Comments**: `# `, `// ` and `-- ` start a comment anywhere in a line. Inside a comment, text between `*…*`, `` `…` `` or `"…"` is highlighted.
- **Emphasis** anywhere else: `*text*` and `` `text` ``.
- **Strings**: `"…"`, `“…”` and `«…»`, with `\` escapes.
- **Blockquotes**: lines starting with `>`.
- **Bullets**: `- `, `* `, `+ ` or `• ` at the start of a line.
- **Brackets**: `()`, `[]` and `{}` pair up within a line, in the body, in comments and in headings, for rainbow brackets and bracket matching. A bracket with no partner on its line stays plain text.
- **URLs**: `https://…` and any other `scheme://…`, in the body and in comments. They keep the color of the text around them, and Nacho2 sets them in italics. They are the links Zed opens when you hold `cmd` (or `alt` if `multi_cursor_modifier` is `cmd_or_ctrl`) and click; a bare `www.…` isn't one. A URL ends at a space, a bracket, a quote or `*`, so trailing punctuation and the brackets around it stay outside it, though a balanced `(…)` inside it, as in Wikipedia links, stays in.
- **Numbers**, R-style **operators** (`<-`, `%in%`, `&&`, …), and `<!-- … -->` comments.

## Navigation

- **Outline**: `cmd-shift-o` lists every heading by its text, nested by level. The outline panel and breadcrumbs show the same tree.
- **Sticky scroll**: with `"sticky_scroll": { "enabled": true }`, the headings above the viewport stay pinned at the top.
- **Folding**: any line with an indented block under it folds, the same way it does in VS Code.
- **Up a level**: this binding moves the cursor to the heading of the section it's in, and from a heading to its parent, one level per press. From a top-level heading it goes to the top of the file. Extensions can't add keybindings, so it goes in `keymap.json`:

  ```json
  {
    "context": "Editor && extension == txt",
    "bindings": {
      "ctrl-cmd-up": [
        "action::Sequence",
        [
          [
            "editor::MoveToBeginningOfLine",
            { "stop_at_soft_wraps": false, "stop_at_indent": false }
          ],
          [
            "editor::MoveToBeginningOfLine",
            { "stop_at_soft_wraps": false, "stop_at_indent": true }
          ],
          "editor::MoveToStartOfLargerSyntaxNode"
        ]
      ]
    }
  }
  ```

  It works because prose isn't a syntax node and a section starts at its heading's `#`, so from the start of a line the next larger node is the section.

## Installing

Zed compiles the grammar itself, so no Rust toolchain is needed.

1. Open the command palette (`cmd-shift-p`) and run **zed: install dev extension**.
2. Choose this directory (`zed-extension-nacho`).

To make `.txt` files open as Nacho, add this to your Zed `settings.json`. The built-in Plain Text language also claims `.txt`, and a `file_types` entry wins over both:

```json
"file_types": {
  "Nacho": ["txt"]
}
```

To use the Nacho2 theme (copy [`nacho2.json`](https://github.com/nachocab/dotfiles/blob/main/zed/themes/nacho2.json) into `~/.config/zed/themes/`) and rainbow brackets:

```json
"theme": { "mode": "system", "light": "One Light", "dark": "Nacho2" },
"colorize_brackets": true
```

Settings that mirror the VS Code `[nacho]` block, plus `cmd-r` for the outline, go in `settings.json` and `keymap.json`:

```json
"languages": {
  "Nacho": {
    "soft_wrap": "editor_width",
    "show_completions_on_input": false,
    "show_edit_predictions": false
  }
}
```

```json
[
  {
    "context": "Editor",
    "bindings": { "cmd-r": "outline::Toggle" }
  }
]
```

## Colors

The Nacho2 theme reproduces the VS Code extension's colors under the VS Code Nacho2 theme, with VS Code's gold, orchid and blue for rainbow brackets. Under other themes, captures fall back to a theme color: each has that color as its first segment and the element after it, so a theme override can restyle one element on its own:

| Element | Capture |
| --- | --- |
| `##` … `########` headings | `function.h2`, `property.h3`, `number.h4`, `string.h5`, `keyword.h6`, `constant.h7`, `type.h8` |
| Blockquote | `keyword.blockquote` |
| Highlight inside a comment | `number.highlight` |
| Emphasis and code elsewhere | `variable.parameter.highlight` |
| Bullet | `punctuation.list_marker` |
| URL in the body, in a comment | `url`, `comment.url` |

For example, to make bullets orange:

```json
"theme_overrides": {
  "Nacho2": {
    "syntax": { "punctuation.list_marker": { "color": "#d19a66" } }
  }
}
```

## Differences from the VS Code extension

- A heading folds only as far as its indentation reaches. Zed has no folding provider for extensions without a language server, so a heading no longer folds away subheadings written at its own indent.
- Strings, emphasis, comment highlights and bracket pairs all close on the line they open on. An unmatched `"` doesn't color the rest of the file.
- `##text` with no space after the hashes is a heading, just as it already was in the VS Code outline.

## Development

The grammar is in `tree-sitter-nacho/`. After editing `grammar.js` or `src/scanner.c`:

```sh
cd tree-sitter-nacho
npm install
npm test   # regenerates src/parser.c and runs test/corpus and test/highlight
```

The highlight tests read `languages/nacho/highlights.scm`, the same file Zed uses.

Zed fetches the grammar from GitHub by commit, so push the grammar change, point `rev` in `extension.toml` at that commit, and reinstall the dev extension.
