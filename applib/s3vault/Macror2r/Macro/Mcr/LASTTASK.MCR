PROC macro:

	REM This macro should be installed under ctrl-*

	REM This macro will act like alt-tab under windows
	REM by pressing ctrl * the last task appears in the upper right corner
	REM of the screen, if the * is released and pressd again (while holding
	REM down the ctrl) the next last task is shown, etc. etc.
	REM By releasing the keys you switch to the task you selected.

	REM Unfortunally almost none of the non stadard applications do
	REM support remote dialogs, therefor the task name will only appear
	REM in the standard applications.

	REM Max Bisschop

local i%, pid%(25), scan%(10), lKey%, swApp%, wt%
cache on

UseApp:("active")
IF Info:(4)=0 : swApp%=1 : ENDIF

call( $d98d, 0, 0, 0, addr( pid%() ) )

i%=2
IF swApp% = 0 : GiPrint:(FIX$(i%,0,2)+": "+Pid2App$:(pid%(i%))+" "+Pid2Fil$:(pid%(i%)))
ELSE : beep 1,800 : ENDIF

lKey%=64 : rem '*'-key pressed
CALL($288e,ADDR(scan%()))

WHILE ( scan%(3) = 128 ) REM control key pressed
	IF lKey% <> scan%(2) and scan%(2) = 64
		lKey%=scan%(2) : CALL($288e,addr(scan%()))
		i%=i%+1 : wt%=0
		IF pid%(i%)=0 : i%=1 : ENDIF
		IF swApp% = 0 : GiPrint:(FIX$(i%,0,2)+": "+Pid2App$:(pid%(i%))+" "+Pid2Fil$:(pid%(i%)))
		ELSE : blink: : ENDIF
	ELSE
		wt%=wt%+1
		if wt% > 1200 : i%=1 : ENDIF REM waited too long
	ENDIF
	lKey%=scan%(2) : CALL($288e,addr(scan%()))
ENDWH

giPrint("")

UsePid:( pid%( i% ) )
ToFront:

ENDP
