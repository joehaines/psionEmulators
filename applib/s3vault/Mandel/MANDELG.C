#include<hwif.h>
#include<p_math.h>

void main(void);
void main_mandel(void);
void SpecificInit(void);

extern unsigned int wMainWid;
int wMainGC;
int wMainGCG;
extern int _UseFullScreen;
extern char *DatStatusNamePtr;

void main(void)
{
	DatStatusNamePtr="MandelG";
	_UseFullScreen=TRUE;
	uCommonInit();
	if(_UseFullScreen==FALSE)
	 p_exit(0);

	uEnableGrey();

	SpecificInit();

	main_mandel();

	p_exit(0);
}


void main_mandel(void)
{
	long zero=0,lvalue,lssize,infin=40;
	int xl,yl,iter;
	double xc,yc,size,halfsize,ssize,ten,four,two,xmin,ymin,step;
	double rpart,ipart,rsqr,isqr,nreal,nimag,modul,swap,temp;

        WMSG_KEY key;
	H_DI_FLOAT dxc={NULL,-4,4};
	H_DI_FLOAT dyc={NULL,-4,4};
	H_DI_FLOAT dsize={NULL,0,4};
	H_DI_NUMBER dinfin={NULL,0,40};

        dxc.value=&xc;
        dyc.value=&yc;
        dsize.value=&size;
        dinfin.value=&infin;

	lssize=160;

	lvalue=10;
	p_longtof(&ten,&lvalue);
	lvalue=4;
	p_longtof(&four,&lvalue);
	lvalue=2;
	p_longtof(&two,&lvalue);

	lvalue=-75;
	p_longtof(&xc,&lvalue);
	p_fdiv(&xc,&ten);
	p_fdiv(&xc,&ten);

	lvalue=0;
	p_longtof(&yc,&lvalue);

	lvalue=25;
	p_longtof(&size,&lvalue);
	p_fdiv(&size,&ten);

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

	p_fld(&halfsize,&size);
	p_fdiv(&halfsize,&two);

	p_fld(&xmin,&xc);
	p_fsub(&xmin,&halfsize);

	p_fld(&ymin,&yc);
	p_fadd(&ymin,&halfsize);

	p_fld(&rpart,&xmin);
	p_fld(&ipart,&ymin);

	p_fld(&step,&size);
	p_longtof(&ssize,&lssize);
	p_fdiv(&step,&ssize);

	for(xl=0;xl<lssize;xl++)
	{
		for(yl=0;yl<lssize;yl++)
		{
			iter=0;
			p_longtof(&nreal,&zero);
			p_longtof(&nimag,&zero);
			p_longtof(&modul,&zero);
			while(p_fcmp(&modul,&four)<0 && iter<infin)
			{
				p_fld(&rsqr,&nreal);
				p_fmul(&rsqr,&nreal);

				p_fld(&isqr,&nimag);
				p_fmul(&isqr,&nimag);

				p_fld(&modul,&rsqr);
				p_fadd(&modul,&isqr);

				p_fld(&swap,&rsqr);
				p_fsub(&swap,&isqr);
				p_fadd(&swap,&rpart);

				p_fld(&temp,&nreal);
				p_fadd(&temp,&nreal);
				p_fmul(&temp,&nimag);
				p_fadd(&temp,&ipart);
				p_fld(&nimag,&temp);

				p_fld(&nreal,&swap);

				iter++;
			}
			if(iter<infin && iter%3!=0)
			{
				if(iter%3==1)
				 gSetGC0(wMainGC);
				else
				 gSetGC0(wMainGCG);

				gDrawLine(xl,yl,xl,yl);
			}

			p_fsub(&ipart,&step);
		}
		p_fadd(&rpart,&step);
		p_fld(&ipart,&ymin);
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

	wd.extent.tl.x=0;
	wd.extent.tl.y=0;
	wd.extent.width=480;
	wd.extent.height=160;

	wSetWindow(wMainWid,W_WIN_EXTENT,&wd);

	gc.gmode=G_TRMODE_SET;
	gc.flags=G_GC_FLAG_GREY_PLANE;

	if((wMainGC=gCreateGC(wMainWid,G_GC_MASK_GMODE,&gc))<0)
	 p_panic(wMainGC);

	if((wMainGCG=gCreateGC(wMainWid,G_GC_MASK_GMODE|G_GC_MASK_GREY,&gc))<0)
	 p_panic(wMainGCG);

	wFlush();
}
