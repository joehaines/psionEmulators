#
# emacs like keybindings for HTML Editor
#

binding "gtkhtml-bindings-emacs"
{
  bind "Home"               { "cursor_move" (left,  all) }
  bind "End"                { "cursor_move" (right, all) }
  bind "<Alt>less"          { "cursor_move" (up,    all) }
  bind "<Alt>greater"       { "cursor_move" (down,  all) }
  bind "<Ctrl>a"            { "cursor_move" (left,  all) }
  bind "<Ctrl>e"            { "cursor_move" (right, all) }
  bind "<Ctrl>b"            { "cursor_move" (left,  one) }
  bind "<Ctrl>f"            { "cursor_move" (right, one) }
  bind "<Alt>b"             { "cursor_move" (left,  word) }
  bind "<Alt>f"             { "cursor_move" (right, word) }
  bind "<Ctrl>p"            { "cursor_move" (up,    one) }
  bind "<Ctrl>n"            { "cursor_move" (down,  one) }

  bind "<Alt>v"             { "cursor_move" (up,    page) }
  bind "<Ctrl>v"            { "cursor_move" (down,  page) }

  bind "<Shift>Up"            { "command" (selection-move-up) }
  bind "<Shift>Down"          { "command" (selection-move-down) }
  bind "<Shift>Left"          { "command" (selection-move-left) }
  bind "<Shift>Right"         { "command" (selection-move-right) }
  bind "<Ctrl><Shift>Left"    { "command" (selection-move-prev-word) }
  bind "<Ctrl><Shift>Right"   { "command" (selection-move-next-word) }
  bind "<Alt><Shift>Left"     { "command" (selection-move-prev-word) }
  bind "<Alt><Shift>Right"    { "command" (selection-move-next-word) }

  bind "<Shift>Home"          { "command" (selection-move-bol) }
  bind "<Shift>End"           { "command" (selection-move-eol) }

  bind "<Shift>Page_Up"       { "command" (selection-move-pageup) }
  bind "<Shift>KP_Page_Up"    { "command" (selection-move-pageup) }
  bind "<Shift>Page_Down"     { "command" (selection-move-pagedown) }
  bind "<Shift>KP_Page_Down"  { "command" (selection-move-pagedown) }

  bind "<Ctrl><Shift>Home"    { "command" (selection-move-bod) }
  bind "<Ctrl><Shift>End"     { "command" (selection-move-eod) }

  bind "<Ctrl>Insert"         { "command" (copy) }
  bind "<Ctrl>KP_Insert"      { "command" (copy) }

  bind "<Shift>Delete"        { "command" (cut) }
  bind "<Shift>KP_Delete"     { "command" (cut) }

  bind "<Shift>Insert"        { "command" (paste) }
  bind "<Shift>KP_Insert"     { "command" (paste) }

  bind "<Ctrl>d"              { "command" (delete) }
  bind "<Ctrl>g"              { "command" (disable-selection) }
  bind "<Ctrl>m"              { "command" (insert-paragraph) }
  bind "<Ctrl>j"              { "command" (insert-paragraph) }
  bind "<Ctrl>w"              { "command" (cut) }
  bind "<Alt>w"               { "command" (copy-and-disable-selection) }
  bind "<Ctrl>y"              { "command" (paste) }

  bind "<Ctrl>k"              { "command" (cut-line) }

  bind "<Ctrl><Alt>b"         { "command" (bold-toggle) }
  bind "<Ctrl><Alt>i"         { "command" (italic-toggle) }
  bind "<Ctrl><Alt>u"         { "command" (underline-toggle) }
  bind "<Ctrl><Alt>s"         { "command" (strikeout-toggle) }

  bind "<Ctrl><Alt>l"         { "command" (align-left) }
  bind "<Ctrl><Alt>c"         { "command" (align-center) }
  bind "<Ctrl><Alt>r"         { "command" (align-right) }

  bind "Tab"                  { "command" (insert-tab-or-next-cell) }
  bind "<Ctrl>Tab"            { "command" (indent-more) }
  bind "<Ctrl><Shift>Tab"            { "command" (indent-less) }
  bind "<Shift>Tab"           { "command" (indent-less-or-prev-cell) }

  bind "<Alt>1"              { "command" (size-minus-2) }
  bind "<Alt>2"              { "command" (size-minus-1) }
  bind "<Alt>3"              { "command" (size-plus-0) }
  bind "<Alt>4"              { "command" (size-plus-1) }
  bind "<Alt>5"              { "command" (size-plus-2) }
  bind "<Alt>6"              { "command" (size-plus-3) }
  bind "<Alt>7"              { "command" (size-plus-4) }

  bind "<Alt>c"              { "command" (capitalize-word) }
  bind "<Alt>l"              { "command" (downcase-word) }
  bind "<Alt>u"              { "command" (upcase-word) }

  bind "<Ctrl><Shift>s"      { "command" (spell-suggest) }
  bind "<Ctrl><Shift>p"      { "command" (spell-personal-add) }
  bind "<Ctrl><Shift>n"      { "command" (spell-session-add) }

  bind "<Ctrl><Shift>r"      { "command" (insert-rule) }

  bind "<Ctrl>s"             { "command" (isearch-forward) }
  bind "<Ctrl>r"             { "command" (isearch-backward) }

  bind "<Ctrl>space"         { "command" (selection-mode) }
  bind "<Alt>space"          { "command" (popup-menu) }
  bind "<Alt>Return"         { "command" (property-dialog) }
  bind "<Alt>KP_Enter"       { "command" (property-dialog) }

  bind "<Alt>q"              { "command" (indent-paragraph) }

  bind "<Ctrl><Shift>t"      { "command" (insert-table-1-1) }

  bind "<Ctrl><Shift>c"      { "command" (insert-col-after) }
  bind "<Ctrl><Alt>c"        { "command" (insert-col-before) }
  bind "<Alt><Shift>c"       { "command" (delete-col) }

  bind "<Ctrl><Shift>r"      { "command" (insert-row-after) }
  bind "<Ctrl><Alt>r"        { "command" (insert-row-before) }
  bind "<Alt><Shift>r"       { "command" (delete-row) }

  bind "<Ctrl><Shift>b"      { "command" (inc-border) }
  bind "<Ctrl><Alt>b"        { "command" (dec-border) }
  bind "<Alt><Shift>b"       { "command" (zero-border) }

  bind "<Alt>d"              { "command" (kill-word) }
  bind "<Alt>BackSpace"      { "command" (backward-kill-word) }

  bind "<Ctrl>o"              { "command" (text-color-apply) }

  bind "F12"                  { "command" (redo) }
  bind "F14"                  { "command" (undo) }
  bind "F16"                  { "command" (copy) }
  bind "F18"                  { "command" (paste) }
  bind "F20"                  { "command" (cut) }
  
# no bindings for these right now
#  bind "F11"                  { "command" (stop) }
#  bind "F13"                  { "command" (props) }
#  bind "F17"                  { "command" (open) }
#  bind "F15"                  { "command" (front) }
#  bind "F19"		       { "command" (find) }
}
