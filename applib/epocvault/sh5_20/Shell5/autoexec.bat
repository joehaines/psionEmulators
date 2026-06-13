#sample autoexec.bat file
echo "############################################################################"
echo
banner -f 268435957 -s 3 " Shell 5  "
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
set prompt="Psion[%H]%p>"
