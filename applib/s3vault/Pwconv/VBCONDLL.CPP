#include <windowsx.h>              


extern unsigned _WinAllocFlag;

extern "C"
{

int FAR PASCAL _export LibMain(HINSTANCE hInstance,WORD wDataSegment,WORD wHeapSize,LPSTR lpszCmdLine)
/*
DLL Entry point
*/
	{
    if (GetWinFlags()&WF_ENHANCED)      // only start up if we are in enhanced mode
        {
        _WinAllocFlag |= GMEM_SHARE;    // all mallocs go to GlobalAlloc with this flag

        if (wHeapSize != 0)			    // unlock local heap
            UnlockData(0);

        }
	return 1; 
	}


int FAR PASCAL _export WEP (int bSystemExit)
/*
Minimum WEP
*/
	{
    return 1;
	}


class CVBCallHandler
    {
public:
    HWND hWnd;
    int type;
    };

CVBCallHandler *CurrentHandler;

BOOL WINAPI _export InitHandSys()
    {
    return TRUE;
    }

CVBCallHandler * WINAPI _export InitHandler(HWND hWnd,int type)
    {
    CVBCallHandler *Hand;
    Hand=new CVBCallHandler;
    Hand->hWnd=hWnd;
    Hand->type=type;
    CurrentHandler=Hand;
    return Hand;
    }

BOOL WINAPI _export StatusProcedure(int Percent )
    {
    long res=SendMessage(CurrentHandler->hWnd,WM_USER+CurrentHandler->type,Percent,0L);
    if (res!=0)
        return TRUE;
    return FALSE;
    }

VOID WINAPI _export DeleteHandler(CVBCallHandler *Hand)
    {
    delete Hand;
    }

} // extern "C"
