/*

File: pcx$meth.c
Desc: Gpic dyl to load/save PC Paintbrush PCX files 

*/

#include <plib.h>
#include <wlib.h>
#include <p_crc.h>
#include <gpic$pcx.g>
#include "gpicdyls.h"
#include <appman.g>    /* For cleanup object */

typedef struct
   {
   UBYTE sig;
   UBYTE version;
   UBYTE encoding;
   UBYTE bitpx;
   P_RECT rect;
   WORD hres;
   WORD vres;
   UBYTE clrma[48];
   UBYTE vmode;
   UBYTE nplanes;
   WORD bplin;
   WORD interp;
   UBYTE padding[58];
   } PCX_HEADER;

typedef struct
   {
   UBYTE red;
   UBYTE green;
   UBYTE blue;
   } COLOUR;

#define PCX_MAGIC    0x0a
#define PCX_HDR_SIZE 128
#define PCX_256_COLOURS 0x0c

#define	MAXCOLOURS   	256
#define	MAXPLANES	4
#define	PCX_MAXVAL	255

#define Vpcxhand (VOID *)(GpicObj->gpic.file)
#define Hpcxhand (HANDLE)(GpicObj->gpic.file)

#define Vpalhand (VOID *)(GpicObj->gpic.palhand)
#define Hpalhand (HANDLE)(GpicObj->gpic.palhand)
#define Cpalhand (COLOUR *)(GpicObj->gpic.palhand)

#define Vpcxhead (VOID *)(GpicObj->gpic.pcxhead)
#define Hpcxhead (HANDLE)(GpicObj->gpic.pcxhead)
#define Ppcxhead ((PCX_HEADER *)(GpicObj->gpic.pcxhead))

#define BUF       (UBYTE *)(GpicObj->gpic.inbuf)
#define Vbuf       (VOID *)(GpicObj->gpic.inbuf)
#define Hbuf       (HANDLE)(GpicObj->gpic.inbuf)

#define Uscan       (UBYTE *)(GpicObj->gpic.scanline)
#define Vscan       (VOID *)(GpicObj->gpic.scanline)
#define Hscan       (HANDLE)(GpicObj->gpic.scanline)

#define Upixel       (UBYTE *)(GpicObj->gpic.pixel)
#define Vpixel       (VOID *)(GpicObj->gpic.pixel)
#define Hpixel       (HANDLE)(GpicObj->gpic.pixel)

#define Ufs1       (BYTE *)(GpicObj->gpic.fsbuf1)
#define Vfs1       (VOID *)(GpicObj->gpic.fsbuf1)
#define Hfs1       (HANDLE)(GpicObj->gpic.fsbuf1)
#define Ufs2       (BYTE *)(GpicObj->gpic.fsbuf2)
#define Vfs2       (VOID *)(GpicObj->gpic.fsbuf2)
#define Hfs2       (HANDLE)(GpicObj->gpic.fsbuf2)

#define Iy       (GpicObj->gpic.by)
#define POS      (GpicObj->gpic.datapos)
#define BUFPOS   (UBYTE *)(GpicObj->gpic.bufpos)

/* ********************
General local functions 
******************** */

LOCAL_C VOID CleanUpInit(VOID)
   {
   CleanupObject=p_new(CAT_GPIC_PCX_OLIB,C_CLEANUP);
   p_send(CleanupObject,O_CL_INIT,4);
   }

LOCAL_C VOID CleanUp(VOID)
   {
   p_send(CleanupObject,O_CL_CLEAN_LEVEL);
   p_send(CleanupObject,O_DESTROY);
   CleanupObject=0;
   }

LOCAL_C VOID CleanAddFile(HANDLE hand)
   {
   p_send(CleanupObject,O_CL_ADD,TY_CLEANUP_IOCHAN,hand);
   }

LOCAL_C VOID CleanAddCell(HANDLE hand)
   {
   p_send(CleanupObject,O_CL_ADD,TY_CLEANUP_ALLOC,hand);
   }

LOCAL_C VOID FreeBMaps(GPIMINFO *iminfo)
   {
   if (iminfo->bid)
      {
      wFree((UINT)iminfo->bid);
      iminfo->bid=0;
      }
   if (iminfo->gid)
      {
      wFree((UINT)iminfo->gid);
      iminfo->gid=0;
      }
   }

LOCAL_C VOID OpenOldPcx(TEXT *imname)
/* 
Open an existing pcx file in shared mode. Leaves file open in pcxhand
Loads header and checks if this really is a PCX file
*/
   {
   VOID *ph;

/* Attempt to open our pcx file */
   f_leave(p_open(&ph,imname,P_FSTREAM|P_FOPEN|P_FRANDOM|P_FSHARE));

   Hpcxhand=(HANDLE)ph;
   CleanAddFile(Hpcxhand);

/* Alloc memory for header */
   Vpcxhead=f_alloc(PCX_HDR_SIZE);
   CleanAddCell(Hpcxhead);

/* Attempt to read in the header */
   f_leave(p_read(Vpcxhand,Vpcxhead,PCX_HDR_SIZE));

/* Is it a proper PCX file header ? */

   if ((Ppcxhead->sig!=PCX_MAGIC)||(Ppcxhead->version>5))
      {
      p_leave(GPIC_ERROR_INCORRECT_FILEFORMAT);
      }      

   if (Ppcxhead->encoding!=1)
      {
      p_leave(GPIC_ERROR_CANT_HANDLE);
      }      
      
   if ((Ppcxhead->nplanes>4)&&(Ppcxhead->bitpx==1))
      {
      p_leave(GPIC_ERROR_CANT_HANDLE);
      }      

   if ((Ppcxhead->nplanes!=1)&&(Ppcxhead->bitpx>1))
      {
      p_leave(GPIC_ERROR_CANT_HANDLE);
      }   
/* Could well be one we can deal with... */

   POS=PCX_HDR_SIZE;
   }

/* *****************
Info local functions 
***************** */

/* *****************
Load local functions 
***************** */

LOCAL_C INT IntensFromPalette(UBYTE colour)
   {
   INT intens;
   COLOUR *col;

   col=Cpalhand;
   col+=colour;   
   intens=col->red+col->green+col->blue;
   return(intens/3);   
   }

LOCAL_C VOID FSAddError(INT input,INT output,BYTE *edcur,BYTE *ednext)
   {
   INT error=input-output;
   INT bigerr=(error*3)/8;
   INT smlerr=(error/4);
 
   bigerr=(bigerr>127?127:bigerr);
   bigerr=(bigerr<-127?-127:bigerr);      
   smlerr=(smlerr>127?127:smlerr);
   smlerr=(smlerr<-127?-127:smlerr);     
 
   *(edcur+1)+=bigerr;
   (*ednext)+=bigerr;          /* Note: Adds! */
   *(ednext+1)=smlerr;       /* Note: Sets! */
   }

LOCAL_C VOID FSGrey(INT bit,INT *level,BYTE **edcur,BYTE **ednext,INT *bbyte,INT *gbyte)
   {
   (*level)+=**edcur;

   if (*level<0)
      {
      *level=0;
      }
   else if (*level>256)
      {
      *level=256;
      }

   if (*level<86)
      {
      (*bbyte)|=1<<bit;
      FSAddError(*level,0,*edcur,*ednext);
      }
   else if (*level<171)
      {
      (*gbyte)|=1<<bit;
      FSAddError(*level,128,*edcur,*ednext);
      }
   else
      {
      FSAddError(*level,256,*edcur,*ednext);
      }

   (*ednext)++;
   (*edcur)++;
   }

LOCAL_C VOID FSBnW(INT bit,INT *level,BYTE **edcur,BYTE **ednext,INT *bbyte)
   {
   (*level)+=**edcur;

   if (*level<0)
      {
      *level=0;
      }
   else if (*level>256)
      {
      *level=256;
      }

   if (*level<128)
      {
      (*bbyte)|=1<<bit;
      FSAddError(*level,0,*edcur,*ednext);
      }
   else
      {
      FSAddError(*level,256,*edcur,*ednext);
      }

   (*ednext)++;
   (*edcur)++;
   }

LOCAL_C VOID NoGrey(INT bit,INT *level,INT *bbyte,INT *gbyte)
   {
   if (*level<86)
      {
      (*bbyte)|=1<<bit;
      }
   else if (*level<171)
      {
      (*gbyte)|=1<<bit;
      }
   }

LOCAL_C VOID NoBnW(INT bit,INT *level,INT *bbyte)
   {
   if (*level<128)
      {
      (*bbyte)|=1<<bit;
      }
   }

LOCAL_C VOID LineToBitmap(GPIMINFO *iminfo)
   {
/* Okay, we have the data in the first section of the line buffer */
/* Now we zap that directly to the bitmap(s) 
/* Actually, we're going to build up the data in scanline first */

   UBYTE *pBwStart,*pBwWrite;
   UBYTE *pGyStart,*pGyWrite;
   INT width=iminfo->bhand.size.x;
   INT bit,bbyte,gbyte,done,twid;
   INT level;
   HANDLE seghand;
   LONG pos;
   BYTE *edcur;
   BYTE *ednext;
   UBYTE *pixels=Upixel;

   if (iminfo->flags&GPIC_DITHER_FLOYD)
      {
      edcur=Ufs1;
      ednext=Ufs2;
      *ednext=0;  /* So things start out right */
      }
   
   width=((width+15)>>3)&~1;
   twid=width;

   pBwStart=pBwWrite=Uscan;

   if (iminfo->depth==2)
      {
      pGyStart=pGyWrite=Uscan+width;
      }

/* Loop over the whole scanline in 8 byte chunks */

/* edcur contains the error for the current line */
/* ednext is filled with the error for the next line */

   while (twid--)
      {
      bbyte=0;
      gbyte=0;
      done=0;

      for (bit=0;bit<8;bit++)
         {
         level=IntensFromPalette(*pixels);

         if (iminfo->depth==2)
            {
            if (iminfo->flags&GPIC_DITHER_FLOYD)
               {
               FSGrey(bit,&level,&edcur,&ednext,&bbyte,&gbyte);
               }
            else
               {
               NoGrey(bit,&level,&bbyte,&gbyte);
               }
            }
         else
            {
            if (iminfo->flags&GPIC_DITHER_FLOYD)
               {
               FSBnW(bit,&level,&edcur,&ednext,&bbyte);
               }
            else
               {
               NoBnW(bit,&level,&bbyte);
               }
            }
         pixels++;
         done++;
         
         if (done>iminfo->bhand.size.x)
            {
            break;
            }
         }
   
      *(pBwWrite++)=bbyte;

      if (iminfo->depth==2)
         {
         *(pGyWrite++)=gbyte;
         }
      }

/* Write out lines to segment */

   pos=(LONG)Iy*(LONG)width;

   f_leave(seghand=p_sgopen(iminfo->bhand.seg_name));   
   f_leave(p_sgcopyto(seghand,pos,pBwStart,width));    
   p_sgclose(seghand);

   if (iminfo->depth==2)
      {
      f_leave(seghand=p_sgopen(iminfo->ghand.seg_name));   
      f_leave(p_sgcopyto(seghand,pos,pGyStart,width));    
      p_sgclose(seghand);
      }
   }

LOCAL_C VOID GreyPalette(INT num)
   {
   INT loop;
   UBYTE flag=0;
   COLOUR *col;

/* Are all the palette values zero?? */
   col=Cpalhand;
   for (loop=0;loop<num;loop++)
      {
      if ((col->red!=0)||(col->green!=0)||(col->blue!=0))
         {
         flag=1;
         }
      col++;
      }

/* Arghhh....I hate PCX "format" .... so many faxing exceptions! */
   if (num<16)
      {
      flag=1;
      for (loop=num;loop<16;loop++)
         {
         if ((col->red!=0)||(col->green!=0)||(col->blue!=0))
            {
            flag=0;
            }
         col++;
         }
      }

/* If so, lets make them grey scale values */
   if (!flag)
      {
      col=Cpalhand; 
      for (loop=0;loop<num;loop++)
         {
         col->red=(loop*PCX_MAXVAL)/(num-1);
         col->green=(loop*PCX_MAXVAL)/(num-1);
         col->blue=(loop*PCX_MAXVAL)/(num-1);
         col++;
         }
      }
   }


LOCAL_C VOID GetPalette(GPIMINFO *iminfo)
   {
   if (Ppcxhead->bitpx==8)
      { /* New PCX - 8 bit planes */
      LONG pos=-769L;
      UBYTE head;
      Vpalhand=f_alloc(768);

      CleanAddCell(Hpalhand);
      
      f_seek(Vpcxhand,P_FEND,&pos);
      f_leave(p_read(Vpcxhand,&head,sizeof(UBYTE)));
      if (head!=PCX_256_COLOURS)
         {
         p_leave(GPIC_ERROR_INCORRECT_FILEFORMAT);
         }      
      f_leave(p_read(Vpcxhand,Vpalhand,MAXCOLOURS*3));      
      }
   else 
      { /* Old PCX - 1,2,3 or 4 planes */
      Vpalhand=(VOID *)&(Ppcxhead->clrma);
      }
   GreyPalette(1<<(Ppcxhead->nplanes*Ppcxhead->bitpx));      
   }

LOCAL_C VOID UnpackPixels(VOID)
   {
   INT bpl=Ppcxhead->bplin;
   INT bits;
   UBYTE *scanline=Uscan;
   UBYTE *pixels=Upixel;

   if (Ppcxhead->bitpx==8)
      {
      while (--bpl>=0)
         {
         *pixels++=*scanline++;         
         }
      }
   else if (Ppcxhead->bitpx==4)
      {
      while (--bpl>=0)
         {
         bits = *scanline++;
         *pixels++ = (bits >> 4) & 0x0f;
         *pixels++ = (bits     ) & 0x0f;
         }
      }
   else if (Ppcxhead->bitpx==2)
      {
      while (--bpl>=0)
         {
         bits	= *scanline++;
	      *pixels++	= (bits >> 6) & 0x03;
         *pixels++	= (bits >> 4) & 0x03;
         *pixels++	= (bits >> 2) & 0x03;
         *pixels++	= (bits     ) & 0x03;
         }
      }
   else if (Ppcxhead->bitpx==1)
      {
      while (--bpl>=0)
         {
	      bits	= *scanline++;
	      *pixels++	= ((bits & 0x80) != 0);
	      *pixels++	= ((bits & 0x40) != 0);
	      *pixels++	= ((bits & 0x20) != 0);
	      *pixels++	= ((bits & 0x10) != 0);
	      *pixels++	= ((bits & 0x08) != 0);
	      *pixels++	= ((bits & 0x04) != 0);
	      *pixels++	= ((bits & 0x02) != 0);
	      *pixels++	= ((bits & 0x01) != 0);
         }
      }
   else 
      {
      p_leave(GPIC_ERROR_CORRUPT_FILE);
      } 
   }

LOCAL_C VOID PlanesToPixels(VOID)
   {
   INT i,j,pixbit,bits,mask;
   UBYTE *p;
   INT todo;
   UBYTE *scanline=Uscan;
   UBYTE *pixels=Upixel;

   p=pixels;
   todo=(Ppcxhead->bplin*8)/(Ppcxhead->bitpx);
   while (todo--)
      {
      *p++=0;
      }

   for (i = 0; i < Ppcxhead->nplanes; i++)
      {
	   p = pixels;
	   pixbit = (1 << i);
	   for (j = 0; j < Ppcxhead->bplin; j++)
	      {
	      bits = *scanline++;
	      for (mask = 0x80; mask != 0; mask >>= 1, p++)
            {
	 	      if (bits & mask)
               {
	 	         *p |= pixbit;
               }
            }
         }
      }
   }

LOCAL_C VOID ConvertScanLine(VOID)
   {
   if (Ppcxhead->nplanes==1)
      {
      UnpackPixels();
      }
   else
      {
      PlanesToPixels();      
      }
   }

LOCAL_C VOID ExtractData(UBYTE **pscanpos,INT *psldone,INT *pbfdone)
   {
   UBYTE data;
   INT count;
   
   while (*psldone>0)
      {
      data=*(BUFPOS)++;
      (*pbfdone)++;

      if ((*pbfdone)>(P_FBLKSIZE-2))
         {
         return;
         }

      (POS)++;         

   	if ((data&0xc0)!=0xc0)
	      {
	      *(*pscanpos)++=data;
         (*psldone)--;
         }
      else
         {
         count=data&0x3f;
         data=*(BUFPOS)++;
         (*pbfdone)++;
         (POS)++;         

         if ((*pbfdone)>(P_FBLKSIZE-2))
            {
            (POS)-=2;
            return;
            }


         if (count>*psldone)
            {
            p_leave(GPIC_ERROR_CORRUPT_FILE);
            }
         while (--count>=0)
            {
	         *(*pscanpos)++=data;
            (*psldone)--;
            }
         }
      }
   }

LOCAL_C VOID LoadBlock(VOID)
   {
   LONG pos=POS;
   f_seek(Vpcxhand,P_FABS,&pos);
   f_leave(p_read(Vpcxhand,BUF,P_FBLKSIZE));
   POS=pos;
   }

/* *****************
Save local functions 
***************** */

LOCAL_C VOID CreatePCXHead(GPIMINFO *iminfo)
/* 
Create a suitable PCX header struct
*/
   {
   INT clearpad;
   Ppcxhead->sig=PCX_MAGIC;
   Ppcxhead->version=3;
   Ppcxhead->encoding=1;
   Ppcxhead->bitpx=1;
   Ppcxhead->rect.tl.x=0;
   Ppcxhead->rect.tl.y=0;
   Ppcxhead->rect.br.x=iminfo->bhand.size.x-1;
   Ppcxhead->rect.br.y=iminfo->bhand.size.y-1;
   Ppcxhead->hres=480;  /* Doesn`t matter! */
   Ppcxhead->vres=160;  /* Doesn`t matter! */

   for (clearpad=0;clearpad<48;clearpad++)
      {
      Ppcxhead->clrma[clearpad]=255;
      }

/* Black */          
   Ppcxhead->clrma[0]=0;
   Ppcxhead->clrma[1]=0;
   Ppcxhead->clrma[2]=0;
/* White */
   Ppcxhead->clrma[45]=255;
   Ppcxhead->clrma[46]=255;
   Ppcxhead->clrma[47]=255;

   if ((iminfo->depth==2)&&(iminfo->gid!=0))
      { /* Grey */
      Ppcxhead->clrma[21]=127;
      Ppcxhead->clrma[22]=127;
      Ppcxhead->clrma[23]=127;
      }

   Ppcxhead->vmode=0;
   if ((iminfo->depth==2)&&(iminfo->gid!=0))
      {
      Ppcxhead->nplanes=4;
      }
   else
      {
      Ppcxhead->nplanes=1;
      }
   Ppcxhead->bplin=(iminfo->bhand.size.x + 7)/8;
   Ppcxhead->interp=1;
   for (clearpad=0;clearpad<58;clearpad++)
      {
      Ppcxhead->padding[clearpad]=0;
      }
   }

LOCAL_C VOID OpenNewPcx(TEXT *imname,GPIMINFO *iminfo)
/*
Create a new PCX file
*/
   {
   VOID *ph;

/* Attempt to open our new pcx file */
   f_leave(p_open(&ph,imname,P_FSTREAM|P_FREPLACE|P_FUPDATE));

   Hpcxhand=(HANDLE)ph;
   CleanAddFile(Hpcxhand);

/* Alloc memory for header */
   Vpcxhead=f_alloc(PCX_HDR_SIZE);
   CleanAddCell(Hpcxhead);

   CreatePCXHead(iminfo);

/* Attempt to write out the header */
   f_leave(p_write(Vpcxhand,Vpcxhead,sizeof(PCX_HEADER)));
   }

LOCAL_C VOID FlipByte(INT wbit,INT off)
   {
   UBYTE ing,inb;
   UBYTE out;
   UBYTE bit;
   UBYTE mask;

   out=0;
   inb=*(Uscan+off);
   ing=*(BUF+off);
   for (bit=0;bit<8;bit++)
      {
      mask=128>>bit;
      if ((inb&mask)==mask)
         {
         mask=1<<bit;
         out&=~mask;
         }
      else if ((ing&mask)==mask)
         {
         mask=1<<bit;
         if (wbit==1)
            {
            out|=mask;
            }
         else
            {
            out&=~mask;
            }
         }
      else
         {
         mask=1<<bit;
         out|=mask;
         }
      }  
   *(BUF+off)=out; 
   }

LOCAL_C INT CreatePCXLine(GPIMINFO *iminfo,INT *gx,INT *bx)
/*
We create A PCX line in BUF 
*/
   {
   UWORD size;
   INT len;
   P_POINT peek;

   size=iminfo->bhand.size.x;
   len=Ppcxhead->bplin;
   peek.x=0;
   peek.y=Iy;
   gPeekBit(iminfo->gid,&peek,size,BUF);
   gPeekBit(iminfo->bid,&peek,size,Uscan);

   if ((iminfo->bid)&&(*bx<iminfo->bhand.size.x))
      {
      while (len>=0)
         {
         FlipByte(1,len);
         len--;
         }
      *bx+=size;                  
      return(1);
      }

   if ((iminfo->gid)&&(*gx<iminfo->bhand.size.x))
      {
      while (len>=0)
         {
         FlipByte(1,len);
         len--;
         }
      *gx+=size;                  
      return(1);
      }
   
   if ((iminfo->gid))
      {
      if (*bx==iminfo->bhand.size.x)
         {
         while (len>=0)
            {
            FlipByte(1,len);
            len--;  
         }
         (*bx)++;
         return(1);
         } 
      if (*gx==iminfo->ghand.size.x)
         {
         while (len>=0)
            {
            FlipByte(0,len);
            len--;  
            }
         return(0);
         }
      }

   return(0);
   }

LOCAL_C INT RLEbuf(VOID)
/* 
Note, slight inefficiency if we have a full buffer...
*/
   {
   UBYTE *pInbuf=BUF;
   UBYTE *pOutbuf=Uscan;
   UBYTE byte;
   UBYTE count;
   INT size;

   size=Ppcxhead->bplin;   

   while (size)
      {
      byte=*pInbuf++;
      size--;
      count=1;
      while ((*pInbuf==byte)&&(size!=0))
         {
         count++;
         pInbuf++;
         size--;
         if ((count==63)||(size==0))
            {
            break;
            }
         }
      if ((count>1)||((byte&0xC0)==0xC0))
         {
         *(pOutbuf++)=0xC0|count;
         *(pOutbuf++)=byte;
         }
      else
         {
         *(pOutbuf++)=byte;
         }     
      }    

   return(pOutbuf-Uscan);
   }

LOCAL_C VOID WriteImageData(GPIMINFO *iminfo)
   { /* OK, we write out a line at a time regardless of efficiency */
   INT moreflag=1;
   INT buflen;   
   INT gx=0;
   INT bx=0;

   while (moreflag)
      {
/* Create an uncompressed scan line in BUF - returns 1 if there is more*/
      moreflag=CreatePCXLine(iminfo,&gx,&bx);

/* Compress BUF in place - returns length of compressed buffer */
      buflen=RLEbuf();

/* Write out BUF to file */
      f_leave(p_write(Vpcxhand,Uscan,buflen));
      }

   Iy++;
   }

#pragma ENTER_CALL

/* ******************
Info control function 
****************** */

LOCAL_C INT CheckPcx(TEXT *imname,GPIMINFO *iminfo)
   {
   if (imname==NULL)
      { /* I.e. program is checking for save flags */
      iminfo->flags=0;
      return(0);
      }

   OpenOldPcx(imname);

/* Okay, we have PCX file, lets get the info for the user */
   
/* Firstly, how colour deep is our image? */

   iminfo->depth=Ppcxhead->nplanes*Ppcxhead->bitpx;

/* How large is our image? */

   iminfo->size.x=Ppcxhead->rect.br.x-Ppcxhead->rect.tl.x+1;
   iminfo->size.y=Ppcxhead->rect.br.y-Ppcxhead->rect.tl.y+1;

/* There can be only 1 image.. 
   and the file must begin at the beginning */

   iminfo->num=1;
   iminfo->fileoffset=0;

/* We support dithering! */

   iminfo->flags=0;
   iminfo->flags|=GPIC_SUPPORTS_DITHERING|GPIC_DITHER_NONE|GPIC_DITHER_FLOYD;
   
   return(0);
   }

/* ******************
Load control function 
****************** */

LOCAL_C INT LoadPcxInit(TEXT *imname,GPIMINFO *iminfo)
   {
   INT width;
   INT x;

   OpenOldPcx(imname);

/* Have we already got bitmaps? */
   FreeBMaps(iminfo);

   iminfo->bhand.size.x=Ppcxhead->rect.br.x-Ppcxhead->rect.tl.x+1;
   iminfo->bhand.size.y=Ppcxhead->rect.br.y-Ppcxhead->rect.tl.y+1;

/* Get a pointer to the palette */
   GetPalette(iminfo);

/* Create input buffer */
   Vbuf=f_alloc(P_FBLKSIZE);
   CleanAddCell(Hbuf);
   BUFPOS=BUF;

/* Create scan line buffer */
   Vscan=f_alloc(Ppcxhead->nplanes*Ppcxhead->bplin);
   CleanAddCell(Hscan);

/* Create pixel buffer */

   width=iminfo->bhand.size.x;
   width=((width+15)>>3)&~1;
   width*=8;

   Vpixel=f_alloc(width);
   CleanAddCell(Hpixel);
      
   if (iminfo->flags&GPIC_DITHER_FLOYD)
      {
      Vfs1=f_alloc(width+1);
      CleanAddCell(Hfs1);
      Vfs2=f_alloc(width+1);
      CleanAddCell(Hfs2);

/* Ensure fs1 buffer is empty */
      for (x=0;x<iminfo->bhand.size.x+1;x++) 
         {
         *(Ufs1+x)=0;
         }   
      }

/* Create new bitmap(s) */

/* Open a black bitmap large enough for this image */
   iminfo->bid=gCreateBit(WS_BIT_SEG_ACCESS,&(iminfo->bhand));

   if ((iminfo->depth==2)&&((Ppcxhead->nplanes*Ppcxhead->bitpx)>1))
      {
      iminfo->ghand.size=iminfo->bhand.size;
      iminfo->gid=gCreateBit(WS_BIT_SEG_ACCESS,&(iminfo->ghand));
      }
   else
      {
      iminfo->depth=1;
      }

   iminfo->done=iminfo->bhand.size.y;

   return(0);   
   }

LOCAL_C INT LoadPcxCont(GPIMINFO *iminfo)
   {
   INT sldone=Ppcxhead->nplanes*Ppcxhead->bplin;
   INT bfdone=BUFPOS-BUF;
   UBYTE *scanpos=Uscan;
   BYTE *swapdist;   /* A temp buffer to swap handle's */

   if (Iy<iminfo->bhand.size.y)
      {
      while (sldone>0)
         {
         if (((bfdone)>(P_FBLKSIZE-2))||(bfdone==0))
            {
            LoadBlock();      
            bfdone=0;
            BUFPOS=BUF;
            }
         ExtractData(&scanpos,&sldone,&bfdone);
         }
      ConvertScanLine();

      LineToBitmap(iminfo);
      
      if (iminfo->flags&GPIC_DITHER_FLOYD)
         { /* Swap FS buffers so second buffer becomes current */
         swapdist=Ufs1;
         Ufs1=Ufs2;
         Ufs2=swapdist;
         }

      iminfo->done=++Iy;

      return(GPIC_LOAD_MORE);
      }
   else
      {      
      return(0);
      }
   }

/* ******************
Save control function 
****************** */

LOCAL_C INT SavePcxInit(TEXT *imname,GPIMINFO *iminfo)
   {
   if (iminfo->bid!=0)
      {
/* Write a PCX header */   
      OpenNewPcx(imname,iminfo);
      }
   else
      {
      return(GPIC_ERROR_NO_IMAGES_TO_SAVE);
      }

   BUF=f_alloc((Ppcxhead->bplin+1));
   CleanAddCell(Hbuf);

   Vscan=f_alloc((Ppcxhead->bplin+1)*2);
   CleanAddCell(Hscan);

   iminfo->done=iminfo->bhand.size.y;

   return(0);
   }

LOCAL_C INT SavePcxCont(TEXT *imname,GPIMINFO *iminfo)
   {
   if (Iy==0)
      {
      iminfo->done=0;
      }

   if (Iy<iminfo->bhand.size.y)
      { /* We still have data to write */
      WriteImageData(iminfo);
      iminfo->done=Iy;
      }
   else
      {
      Iy=0;
      return(0);
      }

   return(GPIC_SAVE_MORE);
   }

/* *************
Method functions
************* */

#pragma METHOD_CALL

METHOD VOID gpic_version(
/*
Return info about the DYL
*/
   PR_GPIC *self,
   TEXT *pInfo,
   WORD *pVer)

   {
   TEXT d[20];
   
   d[0]='P';d[1]='C';d[2]='X';d[3]=' ';d[4]='f';d[5]='o';d[6]='r';d[7]='m';
   d[8]='a';d[9]='t';d[10]='\0';
   p_scpy(pInfo,d);
   *pVer=0x100B;
   }


METHOD INT gpic_info(
/*
Return info about the image file pointed at by imname
in iminfo struct.

Return format:

iminfo->size (P_POINT) 
iminfo->depth = number of bit planes 1,2,3,4 or 8
*/
   PR_GPIC *self,
   TEXT *imname,      /* The filename of the image to look at */
   GPIMINFO *iminfo)

   {
   INT err;


   GpicObj=(VOID *)self;

   CleanUpInit();

   err=p_enter3(CheckPcx,imname,iminfo);

   CleanUp();

   if (err)
      {
      return(err);
      }

   return(0);
   }

METHOD INT gpic_load(
/*
Load the image pointed to by imname - image number iminfo->num
Returns 0 for success (and image info in IMINFO struct)
*/
   PR_GPIC *self,
   TEXT *imname,     /* The filename of the image to load */
   GPIMINFO *iminfo)    

   {
   INT err;

   GpicObj=(VOID *)self;

   CleanUpInit();

   err=p_enter3(LoadPcxInit,imname,iminfo);

   if (err)
      {
      CleanUp();
      Iy=0;
      FreeBMaps(iminfo);
      return(err);
      }
  
   return(0);
   }

METHOD INT gpic_doload(
/*
Load the image pointed to by imname - image number iminfo->num
Returns 0 for success (and image info in IMINFO struct)
*/
   PR_GPIC *self,
   GPIMINFO *iminfo)    

   {
   INT err;

   GpicObj=(VOID *)self;

   if (iminfo->done!=-1)
      {  
      err=p_enter2(LoadPcxCont,iminfo);
      }
   else
      {
      err=GPIC_CANCELLED;
      }
   
   if (err==0)
      {
      Iy=0;
      CleanUp();
      }
   else
      { /* Note, not nescissarily an error - might be LOAD_MORE */
      if (err!=GPIC_LOAD_MORE)
         {
         Iy=0;
         FreeBMaps(iminfo);
         CleanUp();
         }   
      return(err);
      }
  
   return(0);
   }

METHOD INT gpic_save(
/*
Saves the current image in the file pointed to by imname,
Pass image info in IMINFO struct. Returns 0 for success.
*/
   PR_GPIC *self,
   TEXT *imname,      /* The filename of the image to save */
   GPIMINFO *iminfo)    

   {
   INT err;

   GpicObj=(VOID *)self;

   CleanUpInit();

   err=p_enter3(SavePcxInit,imname,iminfo);

   if (err)
      {
      CleanUp();
      return(err);
      }
 
   return(0);
   }

METHOD INT gpic_dosave(
/*
Saves the current image in the file pointed to by imname,
Pass image info in IMINFO struct. Returns 0 for success.
*/
   PR_GPIC *self,
   GPIMINFO *iminfo)    

   {
   INT err;

   GpicObj=(VOID *)self;

   if (iminfo->done!=-1)
      {  
      err=p_enter3(SavePcxCont,iminfo,iminfo);
      }
   else
      {
      err=GPIC_CANCELLED;
      }

   if (err==0)
      {
      CleanUp();
      }
   else
      {
      if (err!=GPIC_SAVE_MORE)
         {
         CleanUp();
         }   
      return(err);
      }
 
   return(0);
   }
