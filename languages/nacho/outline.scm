; A section's range covers its subsections, which is how Zed nests them in the
; outline, breadcrumbs and sticky scroll. Items show the heading's text alone.
(section
  (_
    content: (heading_content) @name)) @item

; A heading with no text (`###`) is listed by its marker.
(section
  (_
    marker: (heading_marker) @name
    !content)) @item
