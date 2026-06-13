/*

File: gpicdyls.h
Desc: Global header for all gpic dyls and apps that use them
Note: This is for VERSION 1 gpic dyls

*/

/* Note: GPIMINFO is used to pass info both ways to/from dyl.
If a file is queried, it returns the number of images, and the 
info of the first image. Subsequent calls can find the size of
other images.

If an image is loaded, it returns the bitmap info in the first one
or two (b/g)hand's */

/* Bit fields for flags */
#define GPIC_SUPPORTS_DITHERING 0x01
#define GPIC_DITHER_NONE 0x02
#define GPIC_DITHER_FLOYD 0x04
#define GPIC_COMPRESSION 0x08

/* Error defines */

#define GPIC_COMPLETED 0
#define GPIC_PROCESS_MORE 1
#define GPIC_LOAD_MORE 1
#define GPIC_SAVE_MORE 1
#define GPIC_CANCELLED 2

#define GPIC_ERROR_INCORRECT_FILEFORMAT 3
#define GPIC_ERROR_CORRUPT_FILE 4
#define GPIC_ERROR_TOO_FEW_IMAGES 5
#define GPIC_ERROR_NO_IMAGES_TO_SAVE 6
#define GPIC_ERROR_CANT_HANDLE 7

typedef struct
   {
   W_OPEN_BIT_SEG bhand;   /* Info of black bitmap if loaded */
   INT bid;                /* Id of black bitmap */
   W_OPEN_BIT_SEG ghand;   /* Info of grey bitmap if loaded */
   INT gid;                /* Id of grey bitmap */
   
   P_POINT size;           /* Size of image */
   WORD depth;             /* Depth of a queried image */
   WORD num;               /* Number of images in a queried file */
   LONG fileoffset;        /* Picture data is offset in file */

   UWORD flags;            /* Various flags (future expansion) */
   LONG done;              /* Loading/saving status */
   UWORD res;              /* Reserved for internal use */
   } GPIMINFO;

/* So DYL's can be cleaned up if anything goes wrong... */

#define CleanupObject DatApp1
#define GpicObj ((PR_GPIC *)DatApp2)

GLREF_D VOID *DatApp1;
GLREF_D VOID *DatApp2;
