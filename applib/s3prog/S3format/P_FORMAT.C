/*
 * Format the device given on the command line.
 * Copyright(C)1993 Advidea Ventures
 * Portions Copyright(C) Psion Plc.
 *
 * Author  :  Russ Beinder
 * Language:  C/PLIB
 * Created :  93.01.27
 *
 * Designed to be run from MCLINK. Tested with MCLINK v3.0.
 * Must be located in the root directory on the default device.
 *
 * Use the following command format from MCLINK:
 *    RUN P_FORMAT LOC::<device>\<name><ext>
 *
 * Example:
 *    RUN P_FORMAT LOC::A:\MYDISK
 */

#include <plib.h>

GLREF_C TEXT *DatCommandPtr;     /* command line */

/*
 * ============================================================================
 * Routine:  formatDevice
 * Purpose:  Format the given device and assign the given volume name.
 *           Taken from pg 126, Psion 'C' SDK, v1.10
 *   Parms:  name - LOC::<device>\<name><ext>
 * Returns:  none
 * ============================================================================
 */
LOCAL_C VOID formatDevice( TEXT *name ) {

   INT err, i, dummy;
   UWORD count;
   VOID *chan;
   TEXT bb[E_MAX_ERROR_TEXT_SIZE];
   TEXT buf[128] = "Format device ";

   chan = NULL;

   p_scat( buf, name );
   err = p_notify( buf, "Are you sure?", "Abort", "Continue", NULL );
   if ( err == 0 )
      return;

   p_printf( "Formatting %s ", name );

   err = p_open( &chan, name, P_FFORMAT );
   if ( err >= 0 ) {

      err = p_read( chan, &count, 0 );

      if ( err >= 0 ) {

         i = 1;
         err = p_read( chan, &dummy, 0 );
         while ( err >= 0 ) {
            p_print( "\r%u%%", (UWORD)( i++ * 100 / count ));
            err = p_read( chan, &count, 0 );
         }

         if ( err == E_FILE_EOF )
            err = 0;

      }
   }

   p_close( chan );

   if ( err < 0 ) {
      p_errs( &bb[0], err );
      p_printf( "\r\nFormat failed: %s", &bb[0] );
      p_scpy( buf, "Format failed " );
      p_scat( buf, name );
      p_notifyerr( err, buf, "Continue", NULL, NULL );
   } else {
      p_printf( "\r\nFormat complete." );
   }

   p_getch();

}





/*
 * ============================================================================
 * Routine:  main
 * ============================================================================
 */
GLDEF_C INT main(VOID) {

   TEXT *pb;
   TEXT cmdline[129];
   TEXT device[129];

/* Get command line */
/* DatCommandPtr points to a zero terminated string containing the
 * name of the file that is being executed, followed by a length byte
 * preceeded string containing the actual command line.
 */
   pb = DatCommandPtr + p_slen(DatCommandPtr) + 1;
   p_bcpy( cmdline, pb + 1, *pb );
   cmdline[*pb] = 0;

   if ( p_slen( cmdline ) == 0 ) {

      p_printf( "Usage:\r\n P_FORMAT [LOC::]<device>[\<name><ext>]" );
      p_getch();
      return 0;

   } else {

      p_fparse( cmdline, "LOC::", device, NULL );

      formatDevice( device );

      return(0);
   }
}
