; Bracket pairs, for matching and rainbow brackets (`colorize_brackets`).
(group "(" @open ")" @close)
(group "[" @open "]" @close)
(group "{" @open "}" @close)
(link "[" @open "]" @close)
(link "(" @open ")" @close)
(link_uri "(" @open ")" @close)
(comment_link_uri "(" @open ")" @close)
