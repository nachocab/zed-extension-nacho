; A section's range covers its subsections, which is how Zed nests them in the
; outline. Items read as `## Heading`.
(section
  (_
    marker: (heading_marker) @context
    content: (heading_content) @name)) @item

; A heading with no text (`###`) is listed by its marker.
(section
  (_
    marker: (heading_marker) @name
    !content)) @item
