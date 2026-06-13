rem This is a sample Macro for use with MACRO SYSTEM
rem Please read contents before using
rem This MACRO is not supported by the Author of JAM so use at your own risk
rem You will need to make modifications to it before use.


proc macro:
	local ret%,pid%
	beep:(8,750)

REM Turn on Log File

	logon:("JAMacro.wrd")
	pause 200
	beep:(4,1500)

REM Write to Log File using Log:

	log:(datim$)
	log:("JAMV5 Macro Started")


REM (app,1) will kill if locked 
REM (app,0) will NOT kill if locked
REM Apps displaying dialogues and Menus will be killed

REM Quit JAM First

	ret%=quit:("JAMV5",0) REM do not kill if locked

REM JAM is locked during adding entries and during the processing phase

	if ret%<>0
		log:("Could not Quit JAM")
		logoff:
		goto finish::
	endif

REM Now quit any applications which are memory hungry
REM It may be better to do some memory checking instead.
REM Add or remove appplications from here as necessary
REM 2nd param=0 for clean exit or 1 to kill if application is in menu or dialogue

	ret%=quit:("JbData",0) 
	ret%=quit:("Word",0) 
	ret%=quit:("Ace",0) 
	ret%=quit:("World",1)  rem I'm not worried about losing data in this APP
	ret%=quit:("Calc",1)  rem I'm not worried about losing data in this APP
	ret%=quit:("Sheet",0)

REM Now launch JAM

REM Flag file prevents JAM creating window to save memory

	if not exist("M:\OPD\jam5mac.flg")
		log:("Creating Flag File")
		trap create "M:\OPD\jam5mac.flg",D,a$
		if err<>0
			log:("Error creating M:\OPD\JAM5MAC.FLG")
			log:(err$(err))
		else
			trap close
		endif
	endif

REM ######Replace location of Jam and name of your Agenda File here####
	
	
	runapp:("A:\app\jamv5.opa","B:\agn\1996.agn")

finish:: 
	log:(datim$)
	log:("Finished")
	logoff:
	busy off
REM Now you could launch the log file.
REM Which saves having to find it !
REM Would need to close it before next time!
REM Runappc:("Word","M:\wrd\jamacro.wrd")

endp

proc quit:(app$,kill%)

	local pid%,lock%,dialog%,menu%

	Giprint "Checking for "+app$
	Log:("Checking for "+app$)

	pid%=app2pid%:(app$)
	if pid%<>0
		log:("Found "+app$)
		useapp:(app$)
		lock%=info:(1)	REM is app locked?
		dialog%=info:(2)REM is app showing dialogue?	
		menu%=info:(3)	REM is app showing menu?
		log:("Lock="+num$(lock%,3)+" Dialog="+num$(dialog%,3)+" Menu="+num$(menu%,3))

REM kill if kill allowed
		if (dialog%=1 or menu%=1) and kill%=1
			log:("Killing "+app$)
			kill:
		else
REM exit if not locked
			if lock%=0 
				log:("Exiting "+app$)
				exit:
			else
				log:(app$ +" is Locked")
				return 1
			endif
		endif
	endif
endp


