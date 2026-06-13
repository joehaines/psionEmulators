/*
//
// Switch 2.0
//
// adapted by David Palmer from Switch 1.0 by Ren‚ Verschoor
//
//   CIX : rverschoor
//   BBS : PSION Nederland BBS  +31-20-6531075
//
// This program is provided as is.
//
// Copy switch.img to the \IMG or \RUN directory on one of the drives.
//
// Start this small utility and leave it alone.
// Every 5 seconds it will check if a mains adaptor is present.
// If not, the normal auto switch-off period is used.
// If a mains adaptor is present, the S3 won't switch off.
// When you remove the mains the old switch-off period is restored.
// If you change the period while the adaptor is connected, and you
// remove the adaptor, the old period is used instead of the changed value.
//
// When a mains adaptor is connected you will hear 3 beeps, from low to high.
// When a mains adaptor is disconnected you will hear 3 beeps, from high to low.
//
*/

#include <plib.h>
#define TRUE 1
#define FALSE 0

GLDEF_C VOID
main(VOID)
{
  E_SUPPLY power;
  INT period;
  INT mains;
  INT pid;
  WORD Mess, mStatus;

  /* Exit immediately if running already */
  if (p_pidfind("SWITCH.*") != (pid = p_getpid()))
    p_exit(0);

  p_unmarka();			/* activity doesn't block auto switch-off */

  period = p_getauto();		/* remember current period */
  p_supply(&power);		/* get power supply info */
  mains = (power.mainsPresent == 1);	/* get current mains status */
  if (mains)
    p_setauto(0xffff);		/* disable auto switch-off */

  p_minit(1, sizeof(WORD));     /* init inter-process messaging */
  p_onterminate(1);		/* request termination message */
  p_mreceive(&mStatus, &Mess);	/* async message receive */

  p_setpri(pid, 0x6F);          /* drop priority below shell */

  FOREVER
  {
    p_ioyield();		/* check for termination message */
    if (mStatus != E_FILE_PENDING)
      break;

    p_supply(&power);		/* get power supply info */
    if (power.mainsPresent == 1)
    {				/* mains is present */
      if (!mains)
      {				/* mains just connected */
	p_sound(5, 1280);	/* notify user */
	p_sound(5, 640);
	p_sound(5, 320);
	period = p_getauto();	/* remember current period */
	p_setauto(0xffff);	/* disable auto switch-off */
	mains = TRUE;
      }
    }
    else
    {				/* mains not present */
      if (mains)
      {				/* mains just removed */
	p_sound(5, 320);	/* notify user */
	p_sound(5, 640);
	p_sound(5, 1280);
	p_setauto(period);	/* enable auto switch-off */
	mains = FALSE;
      }
    }
    p_sleep(50);		/* wait 5 seconds */
  }

  p_setauto(period);		/* restore original timeout */
  p_exit(0);
}

