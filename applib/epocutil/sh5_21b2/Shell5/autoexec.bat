#
# Simple autoexec.bat file, see extras/autoexec.bat for
# a more complex example.
#
# Name the default font
set font Courier11
echo "############################################################################"
echo
banner -f "Arial15 bold underlined" " Shell 5  "
echo "Customising setup..."
#use the UNIX convention of '$' to represent variables
#use the UNIX convention of '/' as the pathname separator
set -o unixvar unixpath
set path=$path$,.
# Common UNIX cursor manipulation keys
bindkey ctrl-a:start ctrl-b:left ctrl-f:right ctrl-e:end
# History search bindings
bindkey ctrl-n:find-next ctrl-p:find-prev
set history=20
alias ll ls -l
alias m more
alias h history
set prompt="Psion[%H]{%t}%p>"
# Remove log window, as this is no longer done by default
log -r
