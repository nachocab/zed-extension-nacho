# Nacho for Zed

Nacho is a lightweight variation of Markdown designed for note-taking. This extension brings the [VS Code extension](https://github.com/nachocab/vscode-nacho-language)'s highlighting, outline, and folding to [Zed](https://zed.dev).

## Syntax

- **Indented headings**: `##` to `########` (or more) start a heading at any indent. Each heading nests under the nearest shallower heading above it, and every level has its own color.
- **Comments**: `# `, `// ` and `-- ` start a comment anywhere in a line. Inside a comment, text between `*…*`, `` `…` `` or `"…"` is highlighted.
- **Emphasis** anywhere else: `*text*` and `` `text` ``.
- **Strings**: `"…"`, `“…”` and `«…»`, with `\` escapes.
- **Blockquotes**: lines starting with `>`.
- **Numbers**, R-style **operators** (`<-`, `%in%`, `&&`, …), and `<!-- … -->` comments.

## Navigation

- **Outline**: `cmd-shift-o` lists every heading, nested by level. The outline panel shows the same tree.
- **Sticky scroll**: with `"sticky_scroll": { "enabled": true }`, the headings above the viewport stay pinned at the top.
- **Folding**: any line with an indented block under it folds, the same way it does in VS Code.

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

Captures are chosen so the default One Dark theme reproduces the VS Code colors. Each capture has a theme color as its first segment and the element after it, so you can restyle one element on its own:

| Element | Capture |
| --- | --- |
| `##` … `########` headings | `function.h2`, `property.h3`, `number.h4`, `string.h5`, `keyword.h6`, `constant.h7`, `type.h8` |
| Blockquote | `keyword.blockquote` |
| Highlight inside a comment | `number.highlight` |
| Emphasis and code elsewhere | `variable.parameter.highlight` |

For example, to make h8 headings violet:

```json
"theme_overrides": {
  "One Dark": {
    "syntax": { "type.h8": { "color": "#b864eb" } }
  }
}
```

## Differences from the VS Code extension

- A heading folds only as far as its indentation reaches. Zed has no folding provider for extensions without a language server, so a heading no longer folds away subheadings written at its own indent.
- Strings, emphasis and comment highlights all close on the line they open on. An unmatched `"` doesn't color the rest of the file.
- `##text` with no space after the hashes is a heading, just as it already was in the VS Code outline.

## Development

The grammar is in `tree-sitter-nacho/`. After editing `grammar.js` or `src/scanner.c`:

```sh
cd tree-sitter-nacho
npm install
npm test   # regenerates src/parser.c and runs test/corpus and test/highlight
```

The highlight tests read `languages/nacho/highlights.scm`, the same file Zed uses.

Zed fetches the grammar by commit, so once the grammar change is committed, point `rev` in `extension.toml` at that commit and reinstall the dev extension. While the repository is local, `[grammars.nacho].repository` is a `file://` URL; switch it to the GitHub URL before publishing.
