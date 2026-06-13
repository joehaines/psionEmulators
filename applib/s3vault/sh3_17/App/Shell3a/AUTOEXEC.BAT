#sample autoexec.bat file
set status=on
banner "Shell 3a" Ü 10 1
echo "Customising setup..."
#use the UNIX convention of '$' to represent variables
set unixvar=on
#use the UNIX convention of '/' as the pathname separator
set unix=on
#Customise the location of the external commands
set path="loc::b:/app/shell3a,loc::b:/opo,loc::m:/opo,."
#Customise the location of the help files
set helppath="LOC::b:/app/shell3a"
set history=20
alias ll ls -l
alias m more
alias h history
alias help man
set prompt="Psion[%H]%p>"
set cache=10000
alias tiny "set font=1,16"
alias small "set font=1,17"
alias medium "set font=4,0"
