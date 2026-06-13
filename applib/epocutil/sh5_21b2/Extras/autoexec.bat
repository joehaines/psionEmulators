#
# Example autoexec.bat file
#
# The file "buttons.mbm" must be copied to the Shell5
#  installation directory (normally C:\System\Apps\Shell5 or
#  D:\System\Apps\Shell5)
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
# Redefine the look of the toolbar buttons
button -b 1 -n 0 -f $_syspath$/buttons.mbm " Toggle" "   Log"
button -b 2 -n 2 -f $_syspath$/buttons.mbm Enlarge "  Font"
button -b 3 -n 3 -f $_syspath$/buttons.mbm " Shrink" "  Font"
button -b 4 -n 1 -f $_syspath$/buttons.mbm "  Help"
# Define the functions of the toolbar buttons
set button1 @log -t
set button2 @set font +1
set button3 @set font -1
set button4 @help
# Remove log window, as this is no longer done by default
log -r
