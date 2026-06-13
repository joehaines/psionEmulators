#include<hwif.h>
#include<p_math.h>

void main(void);
void main_mandelfp(void);
void SpecificInit(void);

extern unsigned int wMainWid;
extern int wMainGc;
extern int _UseFullScreen;
extern char *DatStatusNamePtr;

void main(void)
{
	DatStatusNamePtr="MandelFP";
	_UseFullScreen=TRUE;
	uCommonInit();
	SpecificInit();

	main_mandelfp();

	p_exit(0);
}


void main_mandelfp(void)
{
	long lssize,infin=40;
	int xl,yl,iter;
	double xc,yc,size,xmin,ymin,step;
	double rpart,ipart,rsqr,isqr,nreal,nimag,modul,swap;

        WMSG_KEY key;
	H_DI_FLOAT dxc={NULL,-4,4};
	H_DI_FLOAT dyc={NULL,-4,4};
	H_DI_FLOAT dsize={NULL,0,4};
	H_DI_NUMBER dinfin={NULL,0,40};

        dxc.value=&xc;
        dyc.value=&yc;
        dsize.value=&size;
        dinfin.value=&infin;

	if(_UseFullScreen)
	 lssize=160;
	else
	 lssize=80;

	xc=-0.75;
	yc=0;
	size=2.5;

	if(uOpenDialog("Mandlebrot C V1.00")<0)
	 return;

	if(uAddDialogItem(H_DIALOG_FLOAT,"X centre [R]",&dxc)<0)
	 return;

	if(uAddDialogItem(H_DIALOG_FLOAT,"Y centre [I]",&dyc)<0)
	 return;

	if(uAddDialogItem(H_DIALOG_FLOAT,"Zoom",&dsize)<0)
	 return;

	if(uAddDialogItem(H_DIALOG_NUMBER,"Infinity at",&dinfin)<0)
	 return;

	if(uRunDialog()<=0)
	 return;

	xmin=xc-size/2;
	ymin=yc+size/2;
	rpart=xmin;
	ipart=ymin;
	step=size/(float)lssize;

	for(xl=0;xl<lssize;xl++)
	{
		for(yl=0;yl<lssize;yl++)
		{
			iter=0;
			nreal=0;
			nimag=0;
			modul=0;

			while(modul<4 && iter<infin)
			{
				rsqr=nreal*nreal;
				isqr=nimag*nimag;
				modul=rsqr+isqr;
				swap=rsqr-isqr+rpart;
				nimag=(2*nreal)*nimag+ipart;
				nreal=swap;
				iter++;
			}
			if(iter<infin && iter%2==0)
			 gDrawLine(xl,yl,xl,yl);

			ipart=ipart-step;
		}
		rpart=rpart+step;
		ipart=ymin;
		wFlush();
	}

	do
        {
		uGetKey(&key);
	} while(key.keycode!=W_KEY_ESCAPE);
}


void SpecificInit(void)
{
	W_WINDATA wd;
	G_GC gc;

	wMainWid=uFindMainWid();

	if(_UseFullScreen)
	{
		wd.extent.tl.x=0;
		wd.extent.tl.y=0;
		wd.extent.width=480;
		wd.extent.height=160;
	}
	else
	{
		wd.extent.tl.x=0;
		wd.extent.tl.y=0;
		wd.extent.width=240;
		wd.extent.height=80;
	}

	wSetWindow(wMainWid,W_WIN_EXTENT,&wd);

	gc.gmode=G_TRMODE_SET;

	if((wMainGc=gCreateGC(wMainWid,G_GC_MASK_GMODE,&gc))<0)
	 p_panic(wMainGc);

	wFlush();
}
