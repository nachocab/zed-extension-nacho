; Each capture's first segment picks the theme color, chosen to match the
; VS Code extension under One Dark. The rest names the element, so a theme
; override can restyle one of them on its own (e.g. `function.h2`).

; Headings, one color per level.
(heading_2) @function.h2
(heading_3) @property.h3
(heading_4) @number.h4
(heading_5) @string.h5
(heading_6) @keyword.h6
(heading_7) @constant.h7
(heading_8) @type.h8

(blockquote) @keyword.blockquote

(list_marker) @punctuation.list_marker

[
  (comment)
  (html_comment)
] @comment

; Highlights inside a comment color their text and leave the delimiters
; comment-colored.
(comment_highlight
  (highlight_content) @number.highlight)

; Highlights in the body color their text and leave the delimiters plain.
(emphasis
  (highlight_content) @variable.parameter.highlight)
(code
  (highlight_content) @variable.parameter.highlight)

; URLs, which Zed opens on a modifier-click, keep the color of the text around
; them: built-in themes have no `url` key, so a body URL stays plain and one in
; a comment takes `comment`. A theme can still style `url` and `comment.url`,
; which Nacho2 sets in italics.
(url) @url
(comment_url) @comment.url

(string) @string
(escape_sequence) @string.escape

(number) @number

(operator) @operator
