/*
   SAYTIME.C -- D.Palmer 1995
   Revised 2/96:
     Dropped p_now2str() - it needs SYS$8087.LDD :-(
     This version released into the public domain.
*/

#include <plib.h>
#include <wlib.h>

#define PRIORITY 0x60
#define VOLUME 4

extern WSERV_SPEC *wserv_channel;
static WSERV_SPEC wserv_spec;

static void
speak_time(void)
{
  char time[8], temp[3];
  const char *p = time;

  {
    ULONG st = p_date();
    P_DAYSEC ds;
    P_DATE dt;

    p_sttods(&st, &ds);
    p_dstodt(&ds, &dt);
    p_atos(time, "%02u%02u", dt.hour, dt.minute);
  }

  temp[0] = '*';
  temp[2] = '\0';
  while (*p)
  {
    temp[1] = *p++;
    if (p_playsoundw(temp, 0, VOLUME)!=0)
      return;
  }
}

void
main(void)
{
  INT pid;

  if (p_pidfind("SAYTIME.*") != (pid = p_getpid()))
    p_exit(0);

  wConnect(&wserv_spec, 0, W_CONNECT_AT_BACK | W_CONNECT_DISABLE_LEAVES);
  if (!wserv_channel)
    p_exit(0);

  wInformOnAll(TRUE);
  p_unmarka();
  p_setpri(pid, PRIORITY);

  FOREVER
  {
    WS_EV event;

    wGetEventWait(&event);
    switch (event.type)
    {
    case WM_FOREGROUND:
      wClientPosition(WS_LAST_CLIENT_POSITION, pid);
      wFlush();
      /* Fall through! */
    case WM_ON:
      speak_time();
      break;
    default:
      ;  /* Ignore the rest */
    }
  }
}

