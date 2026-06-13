/*

File: pic$meth.c
Desc: Gpic dyl to load/save Psion PIC format files (V.easy!)

*/

#include <plib.h>
#include <wlib.h>
#include <p_crc.h>
#include <gpic$pic.g>
#include "gpicdyls.h"
/* #include <appman.g>     For cleanup object */

typedef struct
   {
   UWORD checksum;
   P_POINT size;
   UWORD byte_size;
   ULONG offset; /* This is the end of the true header */
   ULONG real_size; /* This is to handle big bitmaps */
   } WS_PIC_HEADER;

#define Vpichand (VOID *)(GpicObj->gpic.file)
#define Hpichand (HANDLE)(GpicObj->gpic.file)
#define Vbhead   ((VOID *)(GpicObj->gpic.bhead))
#define Hbhead   ((WS_PIC_HEADER *)(GpicObj->gpic.bhead)) 
#define Vghead   ((VOID *)(GpicObj->gpic.ghead))
#define Hghead   ((WS_PIC_HEADER *)(GpicObj->gpic.ghead)) 

#define Iby       GpicObj->gpic.by
#define Igy       GpicObj->gpic.gy

#define BUF       (UBYTE *)(GpicObj->gpic.inbuf)
#define DONE      (LONG)(GpicObj->gpic.done)

#define LEFT      (UINT)(imhead->real_size-DONE)

/* ************
Local functions 
************ */

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
   Iby=0;
   Igy=0;
   }

LOCAL_C VOID CleanUpInit(VOID)
   {
   CleanupObject=p_new(CAT_GPIC_PIC_OLIB,C_CLEANUP);
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

/* ******** COMPRESSION CODE ******************** */

LOCAL_C UINT CompCheck(W_OPEN_BIT_SEG hand,W_OPEN_BIT_SEG targhand,INT cmode)
   {
   HANDLE seg=0;
   HANDLE targ=0;
   UINT out;
   UINT size;
   UINT offset; 
   UINT total;
   UBYTE value;
   UBYTE count;
   UBYTE *buf;
   UBYTE mode;
   INT bufsize;

/* Get size of segment */

   seg=p_sgopen(hand.seg_name);
   size=p_sgsize(seg);   
   size*=16; /* Byte size */
    
   if (cmode==1)
      {
      targ=p_sgopen(targhand.seg_name);
      }

   out=6;
   offset=0;
   total=0;

   while (offset<size)
      {
   /* While there is still data left get 128 bytes into buffer */

      bufsize=(127>(size-offset)?(size-offset):127);
    
      p_sgcopyfr(seg,(LONG)offset,BUF+1,bufsize);
      buf=BUF+1;   

   /* See which type of block we have */
      
      mode=1;
      value=*(buf++);
      for (count=1;count<=bufsize;count++)
         {
         if (bufsize==1)
            {
            mode=0;
            break;
            }
         if (value!=*buf)
            {
            if (count==1)
               {
               mode=0;
               }
            else if (mode==1)
               {
               break;
               }
            }
         else if (mode==0)
            {
            break;
            }
         value=*(buf++);
         }      

      buf=BUF;
      *buf=(mode*0x80)+count;

   /* Add the coding requirement amount to the total */

      if (mode==0)
         {
         total+=count+1;
         }
      else
         {
         total+=2;
         }
      offset+=count;

   /* Output the coding if nescessary */
      if (cmode==1)
         {
         if (mode==0)
            {
            p_sgcopyto(targ,(LONG)out,BUF,count+1);
            out+=count+1;
            }
         else if (mode==1)
            {
            p_sgcopyto(targ,(LONG)out,BUF,2);
            out+=2;
            }
         }

      }   

   /* If not a multiple of 2, add 1 to total */
   if ((total % 2)==1)
      {
      total++;
      if (cmode==1)
         {
         buf=BUF;
         *(buf)=0;
         p_sgcopyto(targ,total,BUF,1);
         }
      }
   
   /* Add header = 6 bytes */
   total+=6;
   
   /* Output header */
   if (cmode==1)
      {
      buf=BUF;
      *(buf++)='C';
      *(buf++)='P';
      *(((UINT *)buf)++)=hand.size.x;
      *(((UINT *)buf)++)=hand.size.y;
      p_sgcopyto(targ,0L,BUF,6);
      }

   if (cmode==1)
      {
      p_sgclose(targ);
      }
   p_sgclose(seg);

   return(total);
   }

LOCAL_C VOID SetupCompression(GPIMINFO *iminfo)
   {
   UINT bsize;

/* Check new compression sizes */
   if (iminfo->bid)
      {
      bsize=CompCheck(iminfo->bhand,iminfo->bhand,0);
   
      GpicObj->gpic.bhand=iminfo->bhand;
      GpicObj->gpic.bid=iminfo->bid;

      /* The new bitmap size is 16 x (bsize/2) */

      iminfo->bhand.size.x=16;
      iminfo->bhand.size.y=bsize/2;

      iminfo->bid=gCreateBit(WS_BIT_SEG_ACCESS,&(iminfo->bhand));
      bsize=CompCheck(GpicObj->gpic.bhand,iminfo->bhand,1); 
      }
   if (iminfo->gid)
      {
      bsize=CompCheck(iminfo->ghand,iminfo->ghand,0);
   
      GpicObj->gpic.ghand=iminfo->ghand;
      GpicObj->gpic.gid=iminfo->gid;

      /* The new bitmap size is 16 x (bsize/2) */

      iminfo->ghand.size.x=16;
      iminfo->ghand.size.y=bsize/2;

      iminfo->gid=gCreateBit(WS_BIT_SEG_ACCESS,&(iminfo->ghand));
      bsize=CompCheck(GpicObj->gpic.ghand,iminfo->ghand,1); 
      }   
   }

LOCAL_C VOID UnsetCompression(GPIMINFO *iminfo)
   {
   if (iminfo->bid)
      {
      wFree(iminfo->bid);
      iminfo->bhand=GpicObj->gpic.bhand;
      iminfo->bid=GpicObj->gpic.bid;
      }
   if (iminfo->gid)
      {
      wFree(iminfo->gid);
      iminfo->ghand=GpicObj->gpic.ghand;
      iminfo->gid=GpicObj->gpic.gid;
      }
   }

LOCAL_C VOID DeCompress(W_OPEN_BIT_SEG hand,W_OPEN_BIT_SEG targhand)
   {
   HANDLE seg=0;
   HANDLE targ=0;

   UINT out;
   UINT size;
   UINT offset; 
   UBYTE value;
   UBYTE count;
   UBYTE mode;
   UBYTE buf;
   UBYTE *pBuf=BUF;
   UINT bitsize;

/* Get size of segment */

   seg=p_sgopen(hand.seg_name);
   size=p_sgsize(seg);
   size*=16;
            
   targ=p_sgopen(targhand.seg_name);
   bitsize=p_sgsize(targ);
   bitsize*=16;

   out=0;
   offset=6;

   while (offset<size)
      {

/* Get 1 byte - see what mode it is */

      p_sgcopyfr(seg,(LONG)offset++,&value,1);
            
      mode=(value&0x80)>>7;
      count=value&0x7F;

      if ((out+count)>(bitsize+1))
         {
         break;
         }

      if (count==0)
         {
         break;
         }

      if (mode==0)
         { /* No compression - count bytes copy directly */
         p_sgcopyfr(seg,(LONG)offset,pBuf,count);
         p_sgcopyto(targ,(LONG)out,pBuf,count);
         offset+=count;  
         out+=count;       
         }  
      else if (mode==1)
         {
         p_sgcopyfr(seg,(LONG)offset++,&value,1);
         for (buf=0;buf<count;buf++)
            {
            *(pBuf+buf)=value;
            } 
         p_sgcopyto(targ,(LONG)out,pBuf,count);
         out+=count;
         }

      }

   p_sgclose(targ);
   p_sgclose(seg);
   }

LOCAL_C VOID HandleUnComp(GPIMINFO *iminfo)
   {
   HANDLE seg;
   UBYTE hbuf[6];
   UBYTE *bhb;
   UINT *ihb;

   if (iminfo->bid)
      {
/* Bitmap has been loaded.... */

      seg=p_sgopen(iminfo->bhand.seg_name);
      p_sgcopyfr(seg,0,&hbuf[0],6);
      p_sgclose(seg);
      
      bhb=&hbuf[0];
      ihb=(UINT*)&hbuf[0];

      if ((*bhb=='C')&&(*(bhb+1)=='P'))
         {
/* Create uncompressed bitmaps ... */

         ihb++;
         GpicObj->gpic.bhand.size.x=*(ihb++);
         GpicObj->gpic.bhand.size.y=*(ihb++);

         GpicObj->gpic.bid=gCreateBit(WS_BIT_SEG_ACCESS,&(GpicObj->gpic.bhand));

/* Decompress loaded bitmap */

         DeCompress(iminfo->bhand,GpicObj->gpic.bhand);

/* Zap loaded bitmap */
         
         wFree(iminfo->bid);
         iminfo->bid=GpicObj->gpic.bid;
         iminfo->bhand=GpicObj->gpic.bhand;
         }
      }
   if (iminfo->gid)
      {
/* Bitmap has been loaded.... */

      seg=p_sgopen(iminfo->ghand.seg_name);
      p_sgcopyfr(seg,0,&hbuf[0],6);
      p_sgclose(seg);
      
      bhb=&hbuf[0];
      ihb=(UINT*)&hbuf[0];

      if ((*bhb=='C')&&(*(bhb+1)=='P'))
         {
/* Create uncompressed bitmaps ... */

         ihb++;
         GpicObj->gpic.ghand.size.x=*(ihb++);
         GpicObj->gpic.ghand.size.y=*(ihb++);

         GpicObj->gpic.gid=gCreateBit(WS_BIT_SEG_ACCESS,&(GpicObj->gpic.ghand));

/* Decompress loaded bitmap */

         DeCompress(iminfo->ghand,GpicObj->gpic.ghand);

/* Zap loaded bitmap */
         
         wFree(iminfo->gid);
         iminfo->gid=GpicObj->gpic.gid;
         iminfo->ghand=GpicObj->gpic.ghand;
         }
      }
   }

/* ********************************************** */

LOCAL_C VOID CreatePicSIG(P_FSIG *sig)
   {
   sig->app_id[0]='P';
   sig->app_id[1]='I';
   sig->app_id[2]='C';
   sig->chk_sum=(UBYTE)('P'+'I'+'C');
   sig->file_vn=0x30;
   sig->app_vn=0x30;
   } 

LOCAL_C LONG FindFile(P_FSIG *validsig)
   {
   LONG pos;
   INT sigpos;
   INT bufsize;
   TEXT buf[P_FBLKSIZE];
   
/* Read through the file trying to match the valid buffer */

   while ((bufsize=p_read(Vpichand,buf,P_FBLKSIZE))!=E_FILE_EOF)
      {
      if ((sigpos=p_bsub(buf,P_FBLKSIZE,validsig,sizeof(P_FSIG)))!=-1)
         {
         pos=0;
         p_seek(Vpichand,P_FCUR,&pos);
         pos+=sigpos-bufsize;
         break;
         }
      pos=-sizeof(P_FSIG);
      p_seek(Vpichand,P_FCUR,&pos);
      pos=0;
      }

   return(pos);  
   }

LOCAL_C VOID OpenOldPic(WORD *num,TEXT *imname,GPIMINFO *iminfo)
/* 
Open an existing pic file in shared mode. Leaves file open in pichand
*/
   {
   P_FSIG picsig,validsig;
   LONG pos;
   VOID *ph;
   
/* Initialise PIC header for comparison */
   CreatePicSIG(&validsig);

/* Attempt to open our pic file */
   f_leave(p_open(&ph,imname,P_FSTREAM|P_FOPEN|P_FRANDOM|P_FSHARE));

   Hpichand=(HANDLE)ph;
   CleanAddFile(Hpichand);

/* Is it at the beginning or what? */
   pos=iminfo->fileoffset=FindFile(&validsig);
   f_seek(Vpichand,P_FABS,&pos);

/* Attempt to read in the header */
   f_leave(p_read(Vpichand,&picsig,sizeof(P_FSIG)));

/* Is it a proper PIC file header ? */
   if (p_bcmp((VOID *)&picsig,sizeof(P_FSIG),(VOID *)&validsig,sizeof(P_FSIG)))
      {
      p_leave(GPIC_ERROR_INCORRECT_FILEFORMAT);
      }      

/* OK, we are satisfied that is a PIC file
   How many images in the PIC file? */

/* Attempt to read in the number of images */
   f_leave(p_read(Vpichand,num,sizeof(WORD)));
   }

LOCAL_C VOID OpenNewPic(WORD num,TEXT *imname)
/* 
Open a new pic file. Leaves file open in pichand
*/
   {
   P_FSIG validsig;    
   VOID *ph;
   
/* Initialise PIC header for comparison */
   CreatePicSIG(&validsig);

/* Attempt to open our pic file */
   f_leave(p_open(&ph,imname,P_FSTREAM|P_FREPLACE|P_FUPDATE));

   Hpichand=(HANDLE)ph;
   CleanAddFile(Hpichand);

/* Attempt to write out the header */
   f_leave(p_write(Vpichand,&validsig,sizeof(P_FSIG)));

/* Attempt to write out the number of images */
   f_leave(p_write(Vpichand,&num,sizeof(WORD)));
   }

LOCAL_C VOID GetPicImageHeader(GPIMINFO *iminfo,WS_PIC_HEADER *imhead)
   {
   WORD count;
   LONG offset;
   UBYTE tbuf[6];   
   UBYTE *tub;
   UINT *tui;

   if (iminfo->num==0)
      {
      count=1;
      }
   else
      {
      count=iminfo->num;
      }
   
   offset=iminfo->fileoffset+(LONG)sizeof(P_FSIG)+(LONG)sizeof(WORD)+(LONG)(count-1)*(LONG)(sizeof(WS_PIC_HEADER)-sizeof(ULONG));

/* Move to the header */
   f_seek(Vpichand,P_FABS,&offset);

/* Read it in */
   f_leave(p_read(Vpichand,imhead,sizeof(WS_PIC_HEADER)-sizeof(ULONG)));         

/* Check the real size of the image */
   imhead->real_size=(LONG)((imhead->size.x+15)/16)*(LONG)(imhead->size.y)*(LONG)(2); 
/* Note, in most cases this should match byte_size */

/* Compression code */
/* 
If the width is 16 pixels, then this could be a compressed PIC 
To check, go to the bitmap data, and get 6 bytes. The first 2
should be 'C'+'P', followed by the width and height in UINTs.
*/
   if ((imhead->size.x==16)&&(imhead->size.y>3))
      {
      tub=&tbuf[0];
      tui=(UINT *)(&tbuf[0]); 
      offset+=(LONG)(imhead->offset)+sizeof(WS_PIC_HEADER)-sizeof(ULONG);
      f_seek(Vpichand,P_FABS,&offset);
      f_leave(p_read(Vpichand,tbuf,6));
      if ((*tub=='C')&&(*(tub+1)=='P'))
         {
         GpicObj->gpic.rwid=*(tui+1);
         GpicObj->gpic.rheight=*(tui+2);
         }   
      }
   else
      {
      GpicObj->gpic.rwid=imhead->size.x;
      GpicObj->gpic.rheight=imhead->size.y;
      }
   }

LOCAL_C UWORD ImageChecksum(W_OPEN_BIT_SEG *bithand,LONG *len)
   {
   UWORD crc=0;
   UBYTE buf[P_FBLKSIZE];
   LONG done;
   UINT read;
   HANDLE seghand;

/* Try to open the bitmap segment */
   f_leave(seghand=p_sgopen(bithand->seg_name));   

   *len=(LONG)((bithand->size.x+15)/16)*(LONG)(bithand->size.y)*(LONG)(2); 
      
   for (done=0;done<*len;)
      {
      read=(P_FBLKSIZE>(UINT)(*len-done)?(UINT)(*len-done):P_FBLKSIZE);
      f_leave(p_sgcopyfr(seghand,done,buf,read));
      p_crc(&crc,buf,read);
      done+=read;
      }

   p_sgclose(seghand);

   return(crc);   
   }   


LOCAL_C VOID CreateImageHeader(WORD num,WS_PIC_HEADER *imhead,W_OPEN_BIT_SEG *imseg,LONG *data)
/*
num=headers after this one
*data=image data before this image
*/
   {
   LONG len;

   imhead->checksum=ImageChecksum(imseg,&len);
   imhead->size=imseg->size;
   imhead->real_size=len;
   imhead->byte_size=(UWORD)len; /* This is crazy! */
   imhead->offset= ((sizeof(WS_PIC_HEADER)-sizeof(ULONG))*num) + *data;

   (*data)+=imhead->real_size;
    
   }

LOCAL_C VOID WritePicImageHeaders(WORD num,GPIMINFO *iminfo)
   {
   LONG data=0;
   WS_PIC_HEADER imhead;

   iminfo->done=0;

   if (iminfo->bid)
      {
      CreateImageHeader(--num,&imhead,&iminfo->bhand,&data);      
      iminfo->done+=imhead.real_size;
      f_leave(p_write(Vpichand,&imhead,sizeof(WS_PIC_HEADER)-sizeof(ULONG)));
      }
   if ((iminfo->gid)&&(iminfo->depth==2))
      {
      CreateImageHeader(--num,&imhead,&iminfo->ghand,&data);      
      iminfo->done+=imhead.real_size;
      f_leave(p_write(Vpichand,&imhead,sizeof(WS_PIC_HEADER)-sizeof(ULONG)));
      }
   }



LOCAL_C VOID WriteImageData(W_OPEN_BIT_SEG *bithand,INT *y,GPIMINFO *iminfo,LONG ssize)
   {
   HANDLE seghand;
   UINT read;

   if (*y==0)
      {
      *y=1;
      DONE=0;
      }

/* Try to open the bitmap segment */
   f_leave(seghand=p_sgopen(bithand->seg_name));   

   read=(P_FBLKSIZE>(UINT)(ssize-DONE)?(UINT)(ssize-DONE):P_FBLKSIZE);
   f_leave(p_sgcopyfr(seghand,DONE,BUF,read)); 
   f_leave(p_write(Vpichand,BUF,read));
   DONE+=(LONG)read;
   (LONG)(iminfo->done)+=(LONG)read;

   if (DONE>=ssize)
      {
      *y=bithand->size.y;
      }

   p_sgclose(seghand);

   }

   
LOCAL_C INT LoadBitmapData(WS_PIC_HEADER *imhead,W_OPEN_BIT_SEG *bithand,INT *y,GPIMINFO *iminfo)
/*
Assumes we are at the end of the header just read
Returns 0 for success or GPIC_ERROR_CORRUPT_FILE for CRC failure!
*/
   {
   HANDLE seghand;
   UINT read;
   LONG start;
   
   if (*y==0)
      {
/* Move to the image data */
      start=(LONG)(imhead->offset);
      start+=(LONG)sizeof(P_FSIG);
      start+=(LONG)sizeof(WORD);
      start+=(LONG)(iminfo->num)*(LONG)(sizeof(WS_PIC_HEADER)-sizeof(ULONG));
      start+=(LONG)(iminfo->fileoffset);

      f_seek(Vpichand,P_FABS,&start);
      *y=1;
      DONE=0;
      GpicObj->gpic.crc=0;
      }

/* Try to open the bitmap segment */
   f_leave(seghand=p_sgopen(bithand->seg_name));   

/* Read the data in */

   f_leave(read=p_read(Vpichand,BUF,(P_FBLKSIZE>(LEFT)?(LEFT):P_FBLKSIZE)));
   f_leave(p_sgcopyto(seghand,DONE,BUF,read));    
/* CRC check */
   p_crc(&(GpicObj->gpic.crc),BUF,read);
   DONE+=read;
   iminfo->done+=read;

   p_sgclose(seghand);

   if (DONE>=imhead->real_size)
      {
      *y=bithand->size.y;
      if (GpicObj->gpic.crc!=imhead->checksum)
         {
         return(GPIC_ERROR_CORRUPT_FILE);
         }
      }

   return(0);   
   }

#pragma ENTER_CALL

LOCAL_C INT CheckPic(TEXT *imname,GPIMINFO *iminfo)
   {
   WORD num;
   WS_PIC_HEADER imhead;
   P_POINT tsize;

   if (imname==NULL)
      {
/*      iminfo->flags=GPIC_COMPRESSION; */
      return(0);
      }

   OpenOldPic(&num,imname,iminfo);

/* Okay dokay. Which image does the user want to know about? */
   if (iminfo->num>num)
      {
      return(GPIC_ERROR_TOO_FEW_IMAGES);
      }

   if (iminfo->num==0)
      {
      iminfo->num=1;
      }
   
   GetPicImageHeader(iminfo,&imhead);

/* Hey, if that was a compressed image we checked... */
   tsize.x=GpicObj->gpic.rwid;
   tsize.y=GpicObj->gpic.rheight;
      
/* Ok, we have the pic info....however 
   If there is a bitmap after this one the same size - we return this
   as a possible 2 plane image.... */

   iminfo->depth=1;

   if (iminfo->num<num)
      {
      (iminfo->num)++;
      GetPicImageHeader(iminfo,&imhead);
      if ((GpicObj->gpic.rwid==tsize.x)&&(GpicObj->gpic.rheight==tsize.y))
         {
         iminfo->depth=2;
         }
      }
         
   iminfo->num=num;
   iminfo->size=tsize;

   return(0);
   }

LOCAL_C INT LoadPicInit(TEXT *imname,GPIMINFO *iminfo)
   {
   WORD num;
   P_POINT bs;
   P_POINT gs;

   OpenOldPic(&num,imname,iminfo);

/* See which image(s) the user wants */
   if (iminfo->num>num)
      {
      return(GPIC_ERROR_TOO_FEW_IMAGES);
      }
 
   if (iminfo->num==0)
      {
      iminfo->num=1;
      }

/* Have we already got bitmaps? */
   FreeBMaps(iminfo);

/* Create storage for black header */
   
   Vbhead=f_alloc(sizeof(WS_PIC_HEADER));
   CleanAddCell((HANDLE)Vbhead);

   GetPicImageHeader(iminfo,Vbhead);

   bs.x=GpicObj->gpic.rwid; bs.y=GpicObj->gpic.rheight; 

/* Set the size of the image */
   iminfo->bhand.size=Hbhead->size;

/* Open a bitmap large enough for this image */
   iminfo->bid=gCreateBit(WS_BIT_SEG_ACCESS,&(iminfo->bhand));

/* If depth is set to 2 (instead of 1), the user wants black and grey! */

   if (iminfo->depth==2)
      {
      (iminfo->num)++;

/* Create storage for grey header */
   
      Vghead=f_alloc(sizeof(WS_PIC_HEADER));
      CleanAddCell((HANDLE)Vghead);

      GetPicImageHeader(iminfo,Vghead);
     
      gs.x=GpicObj->gpic.rwid; gs.y=GpicObj->gpic.rheight; 

      if ((gs.x==bs.x)&&(gs.y==bs.y))
         {      
/* Set the size of the grey image */
         iminfo->ghand.size=Hghead->size;

/* Open a grey bitmap large enough for this image */
         iminfo->gid=gCreateBit(WS_BIT_SEG_ACCESS,&(iminfo->ghand));
         }
      else
         {
         iminfo->depth=1;
         }

      (iminfo->num)--;
      }

/* Create storage for input buffer */

   BUF=f_alloc(P_FBLKSIZE);
   CleanAddCell((HANDLE)BUF);

   iminfo->done=(LONG)(Hbhead->real_size);
   if (iminfo->depth==2)   
      {
      iminfo->done+=(LONG)(Hghead->real_size);
      }

   return(0);   
   }

LOCAL_C INT LoadPicCont(GPIMINFO *iminfo)
   {
   INT err;

   if ((Iby==0)&&(Igy==0))
      {
      iminfo->done=0;
      }

   if (Iby<iminfo->bhand.size.y)
      { /* Ok, we're still doing the black plane */
      if ((err=LoadBitmapData(Hbhead,&(iminfo->bhand),&Iby,iminfo))!=0)
         { /* CRC failure or something */
         return(err);
         }
      }
   else if ((iminfo->depth==2)&&(Igy<iminfo->ghand.size.y))
      { /* We're doing the grey plane */
      if (Igy==0)
         {
         iminfo->num++;
         }
      if ((err=LoadBitmapData(Hghead,&(iminfo->ghand),&Igy,iminfo))!=0)
         { /* CRC failure or something */ 
         return(err);
         }
      }
   else
      {
      HandleUnComp(iminfo);
      Igy=0;
      Iby=0;
      return(0);   
      }

   return(GPIC_LOAD_MORE);
   }


LOCAL_C INT SavePicInit(TEXT *imname,GPIMINFO *iminfo)
   {
   WORD num=0;

   if (iminfo->bid)
      {
      num++;
      }
         
   if ((iminfo->gid)&&(iminfo->depth==2))
      {
      num++;
      }

   BUF=f_alloc(P_FBLKSIZE);
   CleanAddCell((HANDLE)BUF);

   if (num)
      {

/* If compression wanted - do the compression stuff here! */
      if (iminfo->flags&GPIC_COMPRESSION)
         {
         SetupCompression(iminfo);
         }
   
/* Write a PIC header */   
      OpenNewPic(num,imname);

/* Sort out the image headers */ 
      WritePicImageHeaders(num,iminfo);
      }
   else
      {
      return(GPIC_ERROR_NO_IMAGES_TO_SAVE);
      }

/* Create storage for input buffer */

   
   return(0);
   }

LOCAL_C INT SavePicCont(GPIMINFO *iminfo)
   {
   LONG ssize;

   if ((Iby==0)&&(Igy==0))
      {
      iminfo->done=0;
      }

   if (Iby<iminfo->bhand.size.y)
      { /* Ok, we're still doing the black plane */
      ssize=(LONG)((iminfo->bhand.size.x+15)/16)*(LONG)(iminfo->bhand.size.y)*(LONG)(2);
      WriteImageData(&(iminfo->bhand),&Iby,iminfo,ssize);      
      }
   else if ((iminfo->depth==2)&&(Igy<iminfo->ghand.size.y))
      { /* We're doing the grey plane */
      ssize=(LONG)((iminfo->ghand.size.x+15)/16)*(LONG)(iminfo->ghand.size.y)*(LONG)(2);
      WriteImageData(&(iminfo->ghand),&Igy,iminfo,ssize);      
      }
   else
      {
      Igy=0;
      Iby=0;

      if (iminfo->flags&GPIC_COMPRESSION)
         {
         UnsetCompression(iminfo); 
         } 

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
   
   d[0]='P';d[1]='s';d[2]='i';d[3]='o';d[4]='n';d[5]=' ';d[6]='P';d[7]='I';
   d[8]='C';d[9]=' ';d[10]='f';d[11]='o';d[12]='r';d[13]='m';
   d[14]='a';d[15]='t';d[16]='\0';
   p_scpy(pInfo,d);
   *pVer=0x101B;
   }


METHOD INT gpic_info(
/*
Return info about the image file pointed at by imname
in iminfo struct.

Return format:

iminfo->size (P_POINT) 

if iminfo->num = 0 then size of first image in bitmap
else size of image num. Error if there aren`t num images!

iminfo->depth = 1 (because PIC images are single plane bitmaps)
ininfo->num = word following P_FSIG header in file

*/
   PR_GPIC *self,
   TEXT *imname,      /* The filename of the image to look at */
   GPIMINFO *iminfo)

   {
   INT err;
      
   GpicObj=(VOID *)self;

   CleanUpInit();

   err=p_enter3(CheckPic,imname,iminfo);

   CleanUp();

   if (err)
      {
      return(err);
      }

   return(0);
   }

METHOD INT gpic_load(
/*
Initialise image loading
Load the image pointed to by imname - image number iminfo->num
Returns 0 for success or an error value.
*/
   PR_GPIC *self,
   TEXT *imname,     /* The filename of the image to load */
   GPIMINFO *iminfo)    

   {
   INT err;

   GpicObj=(VOID *)self;
  
   CleanUpInit();

   err=p_enter3(LoadPicInit,imname,iminfo);

   if (err)
      {
      CleanUp();
      FreeBMaps(iminfo);
      return(err);
      }
  
   return(0);
   }

METHOD INT gpic_doload(
/*
Load the image pointed to by imname - image number iminfo->num
Returns 0 if finished
*/
   PR_GPIC *self,
   GPIMINFO *iminfo)    

   {
   INT err;

   GpicObj=(VOID *)self;

   if (iminfo->done!=-1)
      {  
      err=p_enter2(LoadPicCont,iminfo);
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
      { /* Note, not nescissarily an error - might be LOAD_MORE */
      if (err!=GPIC_LOAD_MORE)
         {
         FreeBMaps(iminfo);
         CleanUp();
         }   
      return(err);
      }
  
   return(0);
   }


METHOD INT gpic_save(
/*
Initialise save.
*/
   PR_GPIC *self,
   TEXT *imname,      /* The filename of the image to save */
   GPIMINFO *iminfo)    

   {
   INT err;

   GpicObj=(VOID *)self;

   CleanUpInit();

   err=p_enter3(SavePicInit,imname,iminfo);

   if (err)
      {
      CleanUp();
      Igy=0;
      Iby=0;
      return(err);
      }
 
   return(0);
   }

METHOD INT gpic_dosave(
/*
Repeat call to process initialised save
Returns 0 for finished.
*/
   PR_GPIC *self,
   GPIMINFO *iminfo)    

   {
   INT err;

   GpicObj=(VOID *)self;

   if (iminfo->done!=-1)
      {  
      err=p_enter2(SavePicCont,iminfo);
      }
   else
      {
      if (iminfo->flags&GPIC_COMPRESSION)
         {
         UnsetCompression(iminfo);
         }
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
         Igy=0;
         Iby=0;
         }   
      return(err);
      }
 
   return(0);
   }
