#include "image.h"
#include "draw.h"
#include "log.h"

#include <limits.h>

void
i_mmarray_cr(i_mmarray *ar,int l) {
  int i;

  ar->lines=l;
  ar->data=mymalloc(sizeof(minmax)*l);
  for(i=0;i<l;i++) { ar->data[i].max=-1; ar->data[i].min=MAXINT; }
}

void
i_mmarray_dst(i_mmarray *ar) {
  ar->lines=0;
  if (ar->data != NULL) { myfree(ar->data); ar->data=NULL; }
}

void
i_mmarray_add(i_mmarray *ar,int x,int y) {
  if (y>-1 && y<ar->lines)
    {
      if (x<ar->data[y].min) ar->data[y].min=x;
      if (x>ar->data[y].max) ar->data[y].max=x;
    }
}

int
i_mmarray_gmin(i_mmarray *ar,int y) {
  if (y>-1 && y<ar->lines) return ar->data[y].min;
  else return -1;
}

int
i_mmarray_getm(i_mmarray *ar,int y) {
  if (y>-1 && y<ar->lines) return ar->data[y].max;
  else return MAXINT;
}

void
i_mmarray_render(i_img *im,i_mmarray *ar,i_color *val) {
  int i,x;
  for(i=0;i<ar->lines;i++) if (ar->data[i].max!=-1) for(x=ar->data[i].min;x<ar->data[i].max;x++) i_ppix(im,x,i,val);
}

void
i_mmarray_render_fill(i_img *im,i_mmarray *ar,i_fill_t *fill) {
  int x, w, y;
  if (im->bits == i_8_bits && fill->fill_with_color) {
    i_color *line = mymalloc(sizeof(i_color) * im->xsize);
    i_color *work = NULL;
    if (fill->combine)
      work = mymalloc(sizeof(i_color) * im->xsize);
    for(y=0;y<ar->lines;y++) {
      if (ar->data[y].max!=-1) {
        x = ar->data[y].min;
        w = ar->data[y].max-ar->data[y].min;

        if (fill->combine) 
          i_glin(im, x, x+w, y, line);
        
        (fill->fill_with_color)(fill, x, y, w, im->channels, line, work);
        i_plin(im, x, x+w, y, line);
      }
    }
  
    myfree(line);
    if (work)
      myfree(work);
  }
  else {
    i_fcolor *line = mymalloc(sizeof(i_fcolor) * im->xsize);
    i_fcolor *work = NULL;
    if (fill->combinef)
      work = mymalloc(sizeof(i_fcolor) * im->xsize);
    for(y=0;y<ar->lines;y++) {
      if (ar->data[y].max!=-1) {
        x = ar->data[y].min;
        w = ar->data[y].max-ar->data[y].min;

        if (fill->combinef) 
          i_glinf(im, x, x+w, y, line);
        
        (fill->fill_with_fcolor)(fill, x, y, w, im->channels, line, work);
        i_plinf(im, x, x+w, y, line);
      }
    }
  
    myfree(line);
    if (work)
      myfree(work);
  }
}


static
void
i_arcdraw(int x1, int y1, int x2, int y2, i_mmarray *ar) {
  double alpha;
  double dsec;
  int temp;
  alpha=(double)(y2-y1)/(double)(x2-x1);
  if (fabs(alpha)<1) 
    {
      if (x2<x1) { temp=x1; x1=x2; x2=temp; temp=y1; y1=y2; y2=temp; }
      dsec=y1;
      while(x1<x2)
	{
	  dsec+=alpha;
	  i_mmarray_add(ar,x1,(int)(dsec+0.5));
	  x1++;
	}
    }
  else
    {
      alpha=1/alpha;
      if (y2<y1) { temp=x1; x1=x2; x2=temp; temp=y1; y1=y2; y2=temp; }
      dsec=x1;
      while(y1<y2)
	{
	  dsec+=alpha;
	  i_mmarray_add(ar,(int)(dsec+0.5),y1);
	  y1++;
	}
    }
}

void
i_mmarray_info(i_mmarray *ar) {
  int i;
  for(i=0;i<ar->lines;i++)
  if (ar->data[i].max!=-1) printf("line %d: min=%d, max=%d.\n",i,ar->data[i].min,ar->data[i].max);
}


void
i_arc(i_img *im,int x,int y,float rad,float d1,float d2,i_color *val) {
  i_mmarray dot;
  float f,fx,fy;
  int x1,y1;

  mm_log((1,"i_arc(im* 0x%x,x %d,y %d,rad %.2f,d1 %.2f,d2 %.2f,val 0x%x)\n",im,x,y,rad,d1,d2,val));

  i_mmarray_cr(&dot,im->ysize);

  x1=(int)(x+0.5+rad*cos(d1*PI/180.0));
  y1=(int)(y+0.5+rad*sin(d1*PI/180.0));
  fx=(float)x1; fy=(float)y1;

  /*  printf("x1: %d.\ny1: %d.\n",x1,y1); */
  i_arcdraw(x, y, x1, y1, &dot);

  x1=(int)(x+0.5+rad*cos(d2*PI/180.0));
  y1=(int)(y+0.5+rad*sin(d2*PI/180.0));

  for(f=d1;f<=d2;f+=0.01) i_mmarray_add(&dot,(int)(x+0.5+rad*cos(f*PI/180.0)),(int)(y+0.5+rad*sin(f*PI/180.0)));
  
  /*  printf("x1: %d.\ny1: %d.\n",x1,y1); */
  i_arcdraw(x, y, x1, y1, &dot);

  /*  dot.info(); */
  i_mmarray_render(im,&dot,val);
  i_mmarray_dst(&dot);
}

void
i_arc_cfill(i_img *im,int x,int y,float rad,float d1,float d2,i_fill_t *fill) {
  i_mmarray dot;
  float f,fx,fy;
  int x1,y1;

  mm_log((1,"i_arc_cfill(im* 0x%x,x %d,y %d,rad %.2f,d1 %.2f,d2 %.2f,fill 0x%x)\n",im,x,y,rad,d1,d2,fill));

  i_mmarray_cr(&dot,im->ysize);

  x1=(int)(x+0.5+rad*cos(d1*PI/180.0));
  y1=(int)(y+0.5+rad*sin(d1*PI/180.0));
  fx=(float)x1; fy=(float)y1;

  /*  printf("x1: %d.\ny1: %d.\n",x1,y1); */
  i_arcdraw(x, y, x1, y1, &dot);

  x1=(int)(x+0.5+rad*cos(d2*PI/180.0));
  y1=(int)(y+0.5+rad*sin(d2*PI/180.0));

  for(f=d1;f<=d2;f+=0.01) i_mmarray_add(&dot,(int)(x+0.5+rad*cos(f*PI/180.0)),(int)(y+0.5+rad*sin(f*PI/180.0)));
  
  /*  printf("x1: %d.\ny1: %d.\n",x1,y1); */
  i_arcdraw(x, y, x1, y1, &dot);

  /*  dot.info(); */
  i_mmarray_render_fill(im,&dot,fill);
  i_mmarray_dst(&dot);
}



/* Temporary AA HACK */


typedef int frac;
static  frac float_to_frac(float x) { return (frac)(0.5+x*16.0); }
static   int frac_sub     (frac x)  { return (x%16); }
static   int frac_int     (frac x)  { return (x/16); }
static float frac_to_float(float x) { return (float)x/16.0; }

static 
void
polar_to_plane(float cx, float cy, float angle, float radius, frac *x, frac *y) {
  *x = float_to_frac(cx+radius*cos(angle));
  *y = float_to_frac(cy+radius*sin(angle));
}

static
void
order_pair(frac *x, frac *y) {
  frac t = *x;
  if (t>*y) {
    *x = *y;
    *y = t;
  }
}




static
void
make_minmax_list(i_mmarray *dot, float x, float y, float radius) {
  float angle = 0.0;
  float astep = radius>0.1 ? .5/radius : 10;
  frac cx, cy, lx, ly, sx, sy;

  mm_log((1, "make_minmax_list(dot %p, x %.2f, y %.2f, radius %.2f)\n", dot, x, y, radius));

  polar_to_plane(x, y, angle, radius, &sx, &sy);
  
  for(angle = 0.0; angle<361; angle +=astep) {
    float alpha;
    lx = sx; ly = sy;
    polar_to_plane(x, y, angle, radius, &cx, &cy);
    sx = cx; sy = cy;

    if (fabs(cx-lx) > fabs(cy-ly)) {
      int ccx, ccy;
      if (lx>cx) { 
	ccx = lx; lx = cx; cx = ccx; 
	ccy = ly; ly = cy; cy = ccy; 
      }

      for(ccx=lx; ccx<=cx; ccx++) {
	ccy = ly + ((cy-ly)*(ccx-lx))/(cx-lx);
	i_mmarray_add(dot, ccx, ccy);
      }
    } else {
      int ccx, ccy;

      if (ly>cy) { 
	ccy = ly; ly = cy; cy = ccy; 
	ccx = lx; lx = cx; cx = ccx; 
      }
      
      for(ccy=ly; ccy<=cy; ccy++) {
	if (cy-ly) ccx = lx + ((cx-lx)*(ccy-ly))/(cy-ly); else ccx = lx;
	i_mmarray_add(dot, ccx, ccy);
      }
    }
  }
}

/* Get the number of subpixels covered */

static
int
i_pixel_coverage(i_mmarray *dot, int x, int y) {
  frac minx = x*16;
  frac maxx = minx+15;
  frac cy;
  int cnt = 0;
  
  for(cy=y*16; cy<(y+1)*16; cy++) {
    frac tmin = dot->data[cy].min;
    frac tmax = dot->data[cy].max;

    if (tmax == -1 || tmin > maxx || tmax < minx) continue;
    
    if (tmin < minx) tmin = minx;
    if (tmax > maxx) tmax = maxx;
    
    cnt+=1+tmax-tmin;
  }
  return cnt;
}

void
i_circle_aa(i_img *im, float x, float y, float rad, i_color *val) {
  i_mmarray dot;
  i_color temp;
  int ly;

  mm_log((1, "i_circle_aa(im %p, x %d, y %d, rad %.2f, val %p)\n", im, x, y, rad, val));

  i_mmarray_cr(&dot,16*im->ysize);
  make_minmax_list(&dot, x, y, rad);

  for(ly = 0; ly<im->ysize; ly++) {
    int ix, cy, cnt = 0, minx = INT_MAX, maxx = INT_MIN;

    /* Find the left/rightmost set subpixels */
    for(cy = 0; cy<16; cy++) {
      frac tmin = dot.data[ly*16+cy].min;
      frac tmax = dot.data[ly*16+cy].max;
      if (tmax == -1) continue;

      if (minx > tmin) minx = tmin;
      if (maxx < tmax) maxx = tmax;
    }

    if (maxx == INT_MIN) continue; /* no work to be done for this row of pixels */

    minx /= 16;
    maxx /= 16;
    for(ix=minx; ix<=maxx; ix++) {
      int cnt = i_pixel_coverage(&dot, ix, ly);
      if (cnt>255) cnt = 255;
      if (cnt) { /* should never be true */
	int ch;
	float ratio = (float)cnt/255.0;
	i_gpix(im, ix, ly, &temp);
	for(ch=0;ch<im->channels; ch++) temp.channel[ch] = (unsigned char)((float)val->channel[ch]*ratio + (float)temp.channel[ch]*(1.0-ratio));
	i_ppix(im, ix, ly, &temp);
      }
    }
  }
  i_mmarray_dst(&dot);
}






void
i_box(i_img *im,int x1,int y1,int x2,int y2,i_color *val) {
  int x,y;
  mm_log((1,"i_box(im* 0x%x,x1 %d,y1 %d,x2 %d,y2 %d,val 0x%x)\n",im,x1,y1,x2,y2,val));
  for(x=x1;x<x2+1;x++) {
    i_ppix(im,x,y1,val);
    i_ppix(im,x,y2,val);
  }
  for(y=y1;y<y2+1;y++) {
    i_ppix(im,x1,y,val);
    i_ppix(im,x2,y,val);
  }
}

void
i_box_filled(i_img *im,int x1,int y1,int x2,int y2,i_color *val) {
  int x,y;
  mm_log((1,"i_box_filled(im* 0x%x,x1 %d,y1 %d,x2 %d,y2 %d,val 0x%x)\n",im,x1,y1,x2,y2,val));
  for(x=x1;x<x2+1;x++) for (y=y1;y<y2+1;y++) i_ppix(im,x,y,val);
}

void
i_box_cfill(i_img *im,int x1,int y1,int x2,int y2,i_fill_t *fill) {
  mm_log((1,"i_box_cfill(im* 0x%x,x1 %d,y1 %d,x2 %d,y2 %d,fill 0x%x)\n",im,x1,y1,x2,y2,fill));

  ++x2;
  if (im->bits == i_8_bits && fill->fill_with_color) {
    i_color *line = mymalloc(sizeof(i_color) * (x2 - x1));
    i_color *work = NULL;
    if (fill->combine)
      work = mymalloc(sizeof(i_color) * (x2-x1));
    while (y1 <= y2) {
      if (fill->combine)
        i_glin(im, x1, x2, y1, line);

      (fill->fill_with_color)(fill, x1, y1, x2-x1, im->channels, line, work);
      i_plin(im, x1, x2, y1, line);
      ++y1;
    }
    myfree(line);
    if (work)
      myfree(work);
  }
  else {
    i_fcolor *line = mymalloc(sizeof(i_fcolor) * (x2 - x1));
    i_fcolor *work;
    work = mymalloc(sizeof(i_fcolor) * (x2 - x1));

    while (y1 <= y2) {
      if (fill->combinef)
        i_glinf(im, x1, x2, y1, line);

      (fill->fill_with_fcolor)(fill, x1, y1, x2-x1, im->channels, line, work);
      i_plinf(im, x1, x2, y1, line);
      ++y1;
    }
    myfree(line);
    if (work)
      myfree(work);
  }
}

void
i_draw(i_img *im,int x1,int y1,int x2,int y2,i_color *val) {
  double alpha;
  double dsec;
  int temp;

  mm_log((1,"i_draw(im* 0x%x,x1 %d,y1 %d,x2 %d,y2 %d,val 0x%x)\n",im,x1,y1,x2,y2,val));

  alpha=(double)(y2-y1)/(double)(x2-x1);
  if (fabs(alpha)<1) 
    {
      if (x2<x1) { temp=x1; x1=x2; x2=temp; temp=y1; y1=y2; y2=temp; }
      dsec=y1;
      while(x1<x2)
	{
	  dsec+=alpha;
	  i_ppix(im,x1,(int)(dsec+0.5),val);
	  x1++;
	}
    }
  else
    {
      alpha=1/alpha;
      if (y2<y1) { temp=x1; x1=x2; x2=temp; temp=y1; y1=y2; y2=temp; }
      dsec=x1;
      while(y1<y2)
	{
	  dsec+=alpha;
	  i_ppix(im,(int)(dsec+0.5),y1,val);
	  y1++;
	}
    }
  mm_log((1,"i_draw: alpha=%f.\n",alpha));
}

void
i_line_aa(i_img *im,int x1,int y1,int x2,int y2,i_color *val) {
  i_color tval;
  float alpha;
  float dsec,dfrac;
  int temp,dx,dy,isec,ch;

  mm_log((1,"i_draw(im* 0x%x,x1 %d,y1 %d,x2 %d,y2 %d,val 0x%x)\n",im,x1,y1,x2,y2,val));

  dy=y2-y1;
  dx=x2-x1;

  if (abs(dx)>abs(dy)) { /* alpha < 1 */
    if (x2<x1) { temp=x1; x1=x2; x2=temp; temp=y1; y1=y2; y2=temp; }
    alpha=(float)(y2-y1)/(float)(x2-x1);

    dsec=y1;
    while(x1<=x2) {
      isec=(int)dsec;
      dfrac=dsec-isec;
      /*      dfrac=1-(1-dfrac)*(1-dfrac); */
      /* This is something we can play with to try to get better looking lines */

      i_gpix(im,x1,isec,&tval);
      for(ch=0;ch<im->channels;ch++) tval.channel[ch]=(unsigned char)(dfrac*(float)tval.channel[ch]+(1-dfrac)*(float)val->channel[ch]);
      i_ppix(im,x1,isec,&tval);
      
      i_gpix(im,x1,isec+1,&tval);
      for(ch=0;ch<im->channels;ch++) tval.channel[ch]=(unsigned char)((1-dfrac)*(float)tval.channel[ch]+dfrac*(float)val->channel[ch]);
      i_ppix(im,x1,isec+1,&tval);
      
      dsec+=alpha;
      x1++;
    }
  } else {
    if (y2<y1) { temp=y1; y1=y2; y2=temp; temp=x1; x1=x2; x2=temp; }
    alpha=(float)(x2-x1)/(float)(y2-y1);
    dsec=x1;
    while(y1<=y2) {
      isec=(int)dsec;
      dfrac=dsec-isec;
      /*      dfrac=sqrt(dfrac); */
      /* This is something we can play with */
      i_gpix(im,isec,y1,&tval);
      for(ch=0;ch<im->channels;ch++) tval.channel[ch]=(unsigned char)(dfrac*(float)tval.channel[ch]+(1-dfrac)*(float)val->channel[ch]);
      i_ppix(im,isec,y1,&tval);

      i_gpix(im,isec+1,y1,&tval);
      for(ch=0;ch<im->channels;ch++) tval.channel[ch]=(unsigned char)((1-dfrac)*(float)tval.channel[ch]+dfrac*(float)val->channel[ch]);
      i_ppix(im,isec+1,y1,&tval);

      dsec+=alpha;
      y1++;
    }
  }
}

double
perm(int n,int k) {
  double r;
  int i;
  r=1;
  for(i=k+1;i<=n;i++) r*=i;
  for(i=1;i<=(n-k);i++) r/=i;
  return r;
}


/* Note in calculating t^k*(1-t)^(n-k) 
   we can start by using t^0=1 so this simplifies to
   t^0*(1-t)^n - we want to multiply that with t/(1-t) each iteration
   to get a new level - this may lead to errors who knows lets test it */

void
i_bezier_multi(i_img *im,int l,double *x,double *y,i_color *val) {
  double *bzcoef;
  double t,cx,cy;
  int k,i;
  int lx = 0,ly = 0;
  int n=l-1;
  double itr,ccoef;


  bzcoef=mymalloc(sizeof(double)*l);
  for(k=0;k<l;k++) bzcoef[k]=perm(n,k);
  ICL_info(val);


  /*  for(k=0;k<l;k++) printf("bzcoef: %d -> %f\n",k,bzcoef[k]); */
  i=0;
  for(t=0;t<=1;t+=0.005) {
    cx=cy=0;
    itr=t/(1-t);
    ccoef=pow(1-t,n);
    for(k=0;k<l;k++) {
      /*      cx+=bzcoef[k]*x[k]*pow(t,k)*pow(1-t,n-k); 
	      cy+=bzcoef[k]*y[k]*pow(t,k)*pow(1-t,n-k);*/

      cx+=bzcoef[k]*x[k]*ccoef;
      cy+=bzcoef[k]*y[k]*ccoef;
      ccoef*=itr;
    }
    /*    printf("%f -> (%d,%d)\n",t,(int)(0.5+cx),(int)(0.5+cy)); */
    if (i++) { 
      i_line_aa(im,lx,ly,(int)(0.5+cx),(int)(0.5+cy),val);
    }
      /*     i_ppix(im,(int)(0.5+cx),(int)(0.5+cy),val); */
    lx=(int)(0.5+cx);
    ly=(int)(0.5+cy);
  }
  ICL_info(val);
  myfree(bzcoef);
}



/* Flood fill 

   REF: Graphics Gems I. page 282+

*/

/* This should be moved into a seperate file? */

/* This is the truncation used:
   
   a double is multiplied by 16 and then truncated.
   This means that 0 -> 0
   So a triangle of (0,0) (10,10) (10,0) Will look like it's
   not filling the (10,10) point nor the (10,0)-(10,10)  line segment

*/




#define IMTRUNC(x) ((int)(x*16))


/*
typedef struct {
  short ms,ls;
} pcord;
*/

typedef int pcord;

struct p_point {
  int n;
  pcord x,y;
};

struct p_line  {
  int n;
  pcord x1,y1;
  pcord x2,y2;
  pcord miny,maxy;
};

struct p_slice {
  int n;
  double x;
};

int
p_compy(const struct p_point *p1, const struct p_point *p2) {
  if (p1->y > p2->y) return 1;
  if (p1->y < p2->y) return -1;
  return 0;
}

int
p_compx(const struct p_slice *p1, const struct p_slice *p2) {
  if (p1->x > p2->x) return 1;
  if (p1->x < p2->x) return -1;
  return 0;
}

/* Change this to int? and round right goddamn it! */

double
p_eval_aty(struct p_line *l,pcord y) {
  int t;
  t=l->y2-l->y1;
  if (t) return ( (y-l->y1)*l->x2 + (l->y2-y)*l->x1 )/t;
  return (l->x1+l->x2)/2.0;
}

double
p_eval_atx(struct p_line *l,pcord x) {
  int t;
  t=l->x2-l->x1;
  if (t) return ( (x-l->x1)*l->y2 + (l->x2-x)*l->y1 )/t;
  return (l->y1+l->y2)/2.0;
}


/* Algorithm to count the pixels covered by line going through pixel (x,y)
   in coarse coords.
*/

/*
static int
p_eval_coverage(struct p_line *l, int lc, int x, pcord y1, pcord y2) {

  return 0;
}
*/


/* Antialiasing polygon algorithm 
   specs:
     1. only nice polygons - no crossovers
     2. 1/16 pixel resolution # previously - floating point co-ordinates
     3. full antialiasing ( complete spectrum of blends )
     4. uses hardly any memory
     5. no subsampling phase

   For each interval we must: 
     1. find which lines are in it
     2. order the lines from in increasing x order.
        since we are assuming no crossovers it is sufficent
        to check a single point on each line.
*/

/*
  Terms:
  
    1. Interval:  A vertical segment in which no lines cross nor end.
    2. Scanline:  A physical line, contains 16 subpixels in the horizontal direction
    3. Slice:     A start stop line pair.
  
 */

/* Templine logic: 
   
   The variable tempflush describes if there is anything in the templine array or not.
   
   if tempflush is 0 then the array is clean.
   if tempflush is 1 then the array contains a partial filled scanline
   
 */

/* Rendering of a single start stop pair:

?? REWRITE
   
   The rendering is split in three parts
   1. From the first start pixel to the first stop pixel
   2. Area from the first end pixel to the last start pixel
   3. Area from the first end pixel to the last start pixel

 */


void
i_poly_aa(i_img *im,int l,double *x,double *y,i_color *val) {
  int i,k;			/* Index variables */
  int clc;			/* Index of next item on interval linelist */
  int tx;			/* Coarse x coord within a scanline */
  pcord miny,maxy;		/* Min and max values of the current slice in the subcord system */
  pcord minacy,maxacy;		/* Min and max values of the current scanline bounded by the slice
				   in the subcord system */
  int cscl;			/* Current scanline */
  pcord cc;			/* Current vertical centerpoint of interval */
  int mt1,mt2;
  int minsx,minex,maxsx,maxex;	/* The horizontal stretches of the lines beloning to the current slice within a scanline */
  int *templine;		/* Line accumulator */

  struct p_point *pset;		/* List of points in polygon */
  struct p_line *lset;		/* List of lines in polygon */
  struct p_slice *tllist;	/* List of slices */

  i_color red,blue,yellow;
  red.rgb.r=255;
  red.rgb.g=0;
  red.rgb.b=0;

  blue.rgb.r=0;
  blue.rgb.g=0;
  blue.rgb.b=255;

  yellow.rgb.r=255;
  yellow.rgb.g=255;
  yellow.rgb.b=255;
  
  if ( (pset=mymalloc(sizeof(struct p_point)*l)) == NULL) { m_fatal(2,"malloc failed\n"); return; }
  if ( (lset=mymalloc(sizeof(struct p_line)*l)) == NULL) { m_fatal(2,"malloc failed\n"); return; }
  if ( (tllist=mymalloc(sizeof(struct p_slice)*l)) == NULL) { m_fatal(2,"malloc failed\n"); return; }
  if ( (templine=mymalloc(sizeof(int)*im->xsize)) == NULL) { m_fatal(2,"malloc failed\n"); return; }

  /* insert the lines into the line list */
  
  for(i=0;i<l;i++) {
    pset[i].n=i;
    pset[i].x=IMTRUNC(x[i]);
    pset[i].y=IMTRUNC(y[i]);
    lset[i].n=i;
    lset[i].x1=IMTRUNC(x[i]);
    lset[i].y1=IMTRUNC(y[i]);
    lset[i].x2=IMTRUNC(x[(i+1)%l]);
    lset[i].y2=IMTRUNC(y[(i+1)%l]);
    lset[i].miny=min(lset[i].y1,lset[i].y2);
    lset[i].maxy=max(lset[i].y1,lset[i].y2);
  }
  
  qsort(pset,l,sizeof(struct p_point),(int(*)(const void *,const void *))p_compy);
  
  printf("post point list (sorted in ascending y order)\n");
  for(i=0;i<l;i++) {
    printf("%d [ %d ] %d %d\n",i,pset[i].n,pset[i].x,pset[i].y);
  }
  
  printf("line list\n");
  for(i=0;i<l;i++) {
    printf("%d [ %d ] (%d , %d) -> (%d , %d) yspan ( %d , %d )\n",i,lset[i].n,lset[i].x1,lset[i].y1,lset[i].x2,lset[i].y2,lset[i].miny,lset[i].maxy);
  }

  printf("MAIN LOOP\n\n");

  /* Zero templine buffer */
  /* Templine buffer flushed everytime a scan line ends */
  for(i=0;i<im->xsize;i++) templine[i]=0;
  

  /* loop on intervals */
  for(i=0;i<l-1;i++) {
    cc=(pset[i].y+pset[i+1].y)/2;
    printf("current slice is: %d to %d ( cpoint %d )\n",pset[i].y,pset[i+1].y,cc);
    clc=0;

    /* stuff this in a function ?? */

    /* Check what lines belong to interval */
    for(k=0;k<l;k++) {
      printf("checking line: %d [ %d ] (%d , %d) -> (%d, %d) yspan ( %d , %d )",
	     k,lset[k].n,lset[k].x1,lset[k].y1,lset[k].x2,lset[k].y2,lset[k].miny,lset[k].maxy);
      if (cc >= lset[k].miny && cc <=  lset[k].maxy) {
	if (lset[k].miny == lset[k].maxy) printf(" HORIZONTAL - skipped\n");
	else {
	  printf(" INSIDE\n"); 
	  tllist[clc].x=p_eval_aty(&lset[k],cc);
	  tllist[clc++].n=k;
	}
      } else printf(" OUTSIDE\n");
    }
    
    /* 
       at this point a table of pixels that need special care should
       be generated from the line list - it should be ordered so that only
       one needs to be checked - options: rendering to a list then order - or
       rendering in the right order might be possible to do nicely with the
       following heuristic:
       
       1. Draw leftmost pixel for this line
       2. If preceeding pixel was occupied check next one else go to 1 again.
    */
    
    printf("lines in current interval:");
    for(k=0;k<clc;k++) printf("  %d (%.2f)",tllist[k].n,tllist[k].x);
    printf("\n");
    
    /* evaluate the lines in the middle of the slice */
    
    printf("Sort lines left to right within interval\n");
    qsort(tllist,clc,sizeof(struct p_slice),(int(*)(const void *,const void *))p_compx);
    
    printf("sorted lines in interval - output:");
    for(k=0;k<clc;k++) printf(" %d",tllist[k].n);
    printf("\n");
    
    miny=pset[i].y;
    maxy=pset[i+1].y;

    /* iterate over scanlines */
    for(cscl=(miny)/16;cscl<=maxy/16;cscl++) {
      minacy=max(miny,cscl*16);
      maxacy=min(maxy,cscl*16+15);
      
      printf("Scanline bound %d - %d\n",minacy, maxacy);

      /* iterate over line pairs (slices) within interval */
      for(k=0;k<clc-1;k+=2) {

	mt1=p_eval_aty(&lset[tllist[k].n],minacy);	/* upper corner */
	mt2=p_eval_aty(&lset[tllist[k].n],maxacy);	/* lower corner */
	minsx=min(mt1,mt2);
	minex=max(mt1,mt2);
	mt1=p_eval_aty(&lset[tllist[k+1].n],minacy);	/* upper corner */
	mt2=p_eval_aty(&lset[tllist[k+1].n],maxacy);	/* lower corner */
	maxsx=min(mt1,mt2);
	maxex=max(mt1,mt2);

	printf("minsx: %d minex: %d\n",minsx,minex);
	printf("maxsx: %d maxex: %d\n",maxsx,maxex);
	
	if (minex/16<maxsx/16) printf("Scan slice is simple!\n");
	else printf("Scan slice is complicated!\n");
	
	if (minsx/16 == minex/16) { /* The line starts and ends in the same pixel */
	  printf("Low slant start pixel\n");
	  templine[minsx/16]=(maxacy-minacy+1)*(minex-minsx+1)/2+((minex | 0xF)-minex)*(maxacy-minacy+1);
	} else {
	  for(tx=minsx/16;tx<minex/16+1;tx++) {
	    int minx,maxx,minxy,maxxy;
	    minx=max(minsx,  tx*16   );
	    maxx=min(minex,  tx*16+15);

	    if (minx == maxx) {
	      templine[tx]=(maxacy-minacy+1);
	    } else {
	    
	      minxy=p_eval_atx(&lset[tllist[k].n], minx);
	      maxxy=p_eval_atx(&lset[tllist[k].n], maxx);
	      
	      templine[tx]+=(abs(minxy-maxxy)+1)*(minex-minsx+1)/2; /* The triangle between the points */
	      if (mt1 < mt2) { /* \ slant */
		/* ((minex | 0xF)-minex)*(maxacy-minacy+1); FIXME: unfinished */
		


	      } else {
		templine[tx]+=((minex | 0xF)-minex)*(maxacy-minacy+1);
	      }
	      
	    }
	  }
	}

	for(tx=maxsx/16;tx<maxex/16+1;tx++) templine[tx]+=16*(maxacy-minacy+1);
	
	/* 	for(tx=minex/16+1;tx<maxsx/16;tx++) 0; */
	
	
	printf("line %d: painting  %d - %d\n",cscl,minex/16+1,maxsx/16);
	if ( (minacy != cscl*16) || (maxacy != cscl*16+15) ) {
	  for(tx=minsx/16;tx<maxex/16+1;tx++) {
	    i_ppix(im,tx,cscl,&yellow);
	  }
	}
	else {
	  for(tx=minsx/16;tx<minex/16+1;tx++) i_ppix(im,tx,cscl,&red);
	  for(tx=maxsx/16;tx<maxex/16+1;tx++) i_ppix(im,tx,cscl,&blue);
	  for(tx=minex/16+1;tx<maxsx/16;tx++) i_ppix(im,tx,cscl,val);
	}

      } /* Slices */
    } /* Scanlines */
  } /* Intervals */
} /* Function */







/* Flood fill algorithm - based on the Ken Fishkins (pixar) gem in 
   graphics gems I */

/*
struct stc {
  int mylx,myrx; 
  int dadlx,dadrx;
  int myy;
  int mydirection;
};

Not used code???
*/


struct stack_element {
  int myLx,myRx;
  int dadLx,dadRx;
  int myY;
  int myDirection;
};


/* create the link data to put push onto the stack */

static
struct stack_element*
crdata(int left,int right,int dadl,int dadr,int y, int dir) {
  struct stack_element *ste;
  ste              = mymalloc(sizeof(struct stack_element));
  ste->myLx        = left;
  ste->myRx        = right;
  ste->dadLx       = dadl;
  ste->dadRx       = dadr;
  ste->myY         = y;
  ste->myDirection = dir;
  return ste;
}

/* i_ccomp compares two colors and gives true if they are the same */

static int
i_ccomp(i_color *val1,i_color *val2,int ch) {
  int i;
  for(i=0;i<ch;i++) if (val1->channel[i] !=val2->channel[i]) return 0;
  return 1;
}


static int
i_lspan(i_img *im,int seedx,int seedy,i_color *val) {
  i_color cval;
  while(1) {
    if (seedx-1 < 0) break;
    i_gpix(im,seedx-1,seedy,&cval);
    if (!i_ccomp(val,&cval,im->channels)) break;
    seedx--;
  }
  return seedx;
}

static int
i_rspan(i_img *im,int seedx,int seedy,i_color *val) {
  i_color cval;
  while(1) {
    if (seedx+1 > im->xsize-1) break;
    i_gpix(im,seedx+1,seedy,&cval);
    if (!i_ccomp(val,&cval,im->channels)) break;
    seedx++;
  }
  return seedx;
}

/* Macro to create a link and push on to the list */

#define ST_PUSH(left,right,dadl,dadr,y,dir) { struct stack_element *s = crdata(left,right,dadl,dadr,y,dir); llist_push(st,&s); }

/* pops the shadow on TOS into local variables lx,rx,y,direction,dadLx and dadRx */
/* No overflow check! */
 
#define ST_POP() { struct stack_element *s; llist_pop(st,&s); lx = s->myLx; rx = s->myRx; dadLx = s->dadLx; dadRx = s->dadRx; y = s->myY; direction= s->myDirection; myfree(s); }

#define ST_STACK(dir,dadLx,dadRx,lx,rx,y) { int pushrx = rx+1; int pushlx = lx-1; ST_PUSH(lx,rx,pushlx,pushrx,y+dir,dir); if (rx > dadRx)                                      ST_PUSH(dadRx+1,rx,pushlx,pushrx,y-dir,-dir); if (lx < dadLx) ST_PUSH(lx,dadLx-1,pushlx,pushrx,y-dir,-dir); }

#define SET(x,y) btm_set(btm,x,y);

#define INSIDE(x,y) ((!btm_test(btm,x,y) && ( i_gpix(im,x,y,&cval),i_ccomp(&val,&cval,channels)  ) ))

void
i_flood_fill(i_img *im,int seedx,int seedy,i_color *dcol) {

  int lx,rx;
  int y;
  int direction;
  int dadLx,dadRx;

  int wasIn=0;
  int x=0;

  /*  int tx,ty; */

  int bxmin=seedx,bxmax=seedx,bymin=seedy,bymax=seedy;

  struct llist *st;
  struct i_bitmap *btm;

  int channels,xsize,ysize;
  i_color cval,val;

  channels = im->channels;
  xsize    = im->xsize;
  ysize    = im->ysize;

  btm = btm_new(xsize,ysize);
  st = llist_new(100,sizeof(struct stack_element*));

  /* Get the reference color */
  i_gpix(im,seedx,seedy,&val);

  /* Find the starting span and fill it */
  lx = i_lspan(im,seedx,seedy,&val);
  rx = i_rspan(im,seedx,seedy,&val);
  
  /* printf("span: %d %d \n",lx,rx); */

  for(x=lx; x<=rx; x++) SET(x,seedy);

  ST_PUSH(lx, rx, lx, rx, seedy+1, 1);
  ST_PUSH(lx, rx, lx, rx, seedy-1,-1);

  while(st->count) {
    ST_POP();
    
    if (y<0 || y>ysize-1) continue;

    if (bymin > y) bymin=y; /* in the worst case an extra line */
    if (bymax < y) bymax=y; 

    /*     printf("start of scan - on stack : %d \n",st->count); */

    
    /*     printf("lx=%d rx=%d dadLx=%d dadRx=%d y=%d direction=%d\n",lx,rx,dadLx,dadRx,y,direction); */
    
    /*
    printf(" ");
    for(tx=0;tx<xsize;tx++) printf("%d",tx%10);
    printf("\n");
    for(ty=0;ty<ysize;ty++) {
      printf("%d",ty%10);
      for(tx=0;tx<xsize;tx++) printf("%d",!!btm_test(btm,tx,ty));
      printf("\n");
    }

    printf("y=%d\n",y);
    */


    x=lx+1;
    if ( (wasIn = INSIDE(lx,y)) ) {
      SET(lx,y);
      lx--;
      while(INSIDE(lx,y) && lx > 0) {
	SET(lx,y);
	lx--;
      }
    }

    if (bxmin > lx) bxmin=lx;
    
    while(x <= xsize-1) {
      /*  printf("x=%d\n",x); */
      if (wasIn) {
	
	if (INSIDE(x,y)) {
	  /* case 1: was inside, am still inside */
	  SET(x,y);
	} else {
	  /* case 2: was inside, am no longer inside: just found the
	     right edge of a span */
	  ST_STACK(direction,dadLx,dadRx,lx,(x-1),y);

	  if (bxmax < x) bxmax=x;

	  wasIn=0;
	}
      } else {
	if (x>rx) goto EXT;
	if (INSIDE(x,y)) {
	  SET(x,y);
	  /* case 3: Wasn't inside, am now: just found the start of a new run */
	  wasIn=1;
	    lx=x;
	} else {
	  /* case 4: Wasn't inside, still isn't */
	}
      }
      x++;
    }
  EXT: /* out of loop */
    if (wasIn) {
      /* hit an edge of the frame buffer while inside a run */
      ST_STACK(direction,dadLx,dadRx,lx,(x-1),y);
      if (bxmax < x) bxmax=x;
    }
  }
  
  /*   printf("lx=%d rx=%d dadLx=%d dadRx=%d y=%d direction=%d\n",lx,rx,dadLx,dadRx,y,direction); 
       printf("bounding box: [%d,%d] - [%d,%d]\n",bxmin,bymin,bxmax,bymax); */

  for(y=bymin;y<=bymax;y++) for(x=bxmin;x<=bxmax;x++) if (btm_test(btm,x,y)) i_ppix(im,x,y,dcol);

  btm_destroy(btm);
  mm_log((1, "DESTROY\n"));
  llist_destroy(st);
}

static struct i_bitmap *
i_flood_fill_low(i_img *im,int seedx,int seedy,
                 int *bxminp, int *bxmaxp, int *byminp, int *bymaxp) {
  int lx,rx;
  int y;
  int direction;
  int dadLx,dadRx;

  int wasIn=0;
  int x=0;

  /*  int tx,ty; */

  int bxmin=seedx,bxmax=seedx,bymin=seedy,bymax=seedy;

  struct llist *st;
  struct i_bitmap *btm;

  int channels,xsize,ysize;
  i_color cval,val;

  channels=im->channels;
  xsize=im->xsize;
  ysize=im->ysize;

  btm=btm_new(xsize,ysize);
  st=llist_new(100,sizeof(struct stack_element*));

  /* Get the reference color */
  i_gpix(im,seedx,seedy,&val);

  /* Find the starting span and fill it */
  lx=i_lspan(im,seedx,seedy,&val);
  rx=i_rspan(im,seedx,seedy,&val);
  
  /* printf("span: %d %d \n",lx,rx); */

  for(x=lx;x<=rx;x++) SET(x,seedy);

  ST_PUSH(lx,rx,lx,rx,seedy+1,1);
  ST_PUSH(lx,rx,lx,rx,seedy-1,-1);

  while(st->count) {
    ST_POP();
    
    if (y<0 || y>ysize-1) continue;

    if (bymin > y) bymin=y; /* in the worst case an extra line */
    if (bymax < y) bymax=y; 

    /*     printf("start of scan - on stack : %d \n",st->count); */

    
    /*     printf("lx=%d rx=%d dadLx=%d dadRx=%d y=%d direction=%d\n",lx,rx,dadLx,dadRx,y,direction); */
    
    /*
    printf(" ");
    for(tx=0;tx<xsize;tx++) printf("%d",tx%10);
    printf("\n");
    for(ty=0;ty<ysize;ty++) {
      printf("%d",ty%10);
      for(tx=0;tx<xsize;tx++) printf("%d",!!btm_test(btm,tx,ty));
      printf("\n");
    }

    printf("y=%d\n",y);
    */


    x=lx+1;
    if ( (wasIn = INSIDE(lx,y)) ) {
      SET(lx,y);
      lx--;
      while(INSIDE(lx,y) && lx > 0) {
	SET(lx,y);
	lx--;
      }
    }

    if (bxmin > lx) bxmin=lx;
    
    while(x <= xsize-1) {
      /*  printf("x=%d\n",x); */
      if (wasIn) {
	
	if (INSIDE(x,y)) {
	  /* case 1: was inside, am still inside */
	  SET(x,y);
	} else {
	  /* case 2: was inside, am no longer inside: just found the
	     right edge of a span */
	  ST_STACK(direction,dadLx,dadRx,lx,(x-1),y);

	  if (bxmax < x) bxmax=x;

	  wasIn=0;
	}
      } else {
	if (x>rx) goto EXT;
	if (INSIDE(x,y)) {
	  SET(x,y);
	  /* case 3: Wasn't inside, am now: just found the start of a new run */
	  wasIn=1;
	    lx=x;
	} else {
	  /* case 4: Wasn't inside, still isn't */
	}
      }
      x++;
    }
  EXT: /* out of loop */
    if (wasIn) {
      /* hit an edge of the frame buffer while inside a run */
      ST_STACK(direction,dadLx,dadRx,lx,(x-1),y);
      if (bxmax < x) bxmax=x;
    }
  }
  
  /*   printf("lx=%d rx=%d dadLx=%d dadRx=%d y=%d direction=%d\n",lx,rx,dadLx,dadRx,y,direction); 
       printf("bounding box: [%d,%d] - [%d,%d]\n",bxmin,bymin,bxmax,bymax); */

  llist_destroy(st);

  *bxminp = bxmin;
  *bxmaxp = bxmax;
  *byminp = bymin;
  *bymaxp = bymax;

  return btm;
}

void
i_flood_cfill(i_img *im, int seedx, int seedy, i_fill_t *fill) {
  int bxmin, bxmax, bymin, bymax;
  struct i_bitmap *btm;
  int x, y;
  int start;

  btm = i_flood_fill_low(im, seedx, seedy, &bxmin, &bxmax, &bymin, &bymax);

  if (im->bits == i_8_bits && fill->fill_with_color) {
    i_color *line = mymalloc(sizeof(i_color) * (bxmax - bxmin));
    i_color *work = NULL;
    if (fill->combine)
      work = mymalloc(sizeof(i_color) * (bxmax - bxmin));

    for(y=bymin;y<=bymax;y++) {
      x = bxmin;
      while (x < bxmax) {
        while (x < bxmax && !btm_test(btm, x, y)) {
          ++x;
        }
        if (btm_test(btm, x, y)) {
          start = x;
          while (x < bxmax && btm_test(btm, x, y)) {
            ++x;
          }
          if (fill->combine)
            i_glin(im, start, x, y, line);
          (fill->fill_with_color)(fill, start, y, x-start, im->channels, 
                                  line, work);
          i_plin(im, start, x, y, line);
        }
      }
    }
    myfree(line);
    if (work)
      myfree(work);
  }
  else {
    i_fcolor *line = mymalloc(sizeof(i_fcolor) * (bxmax - bxmin));
    i_fcolor *work = NULL;
    if (fill->combinef)
      work = mymalloc(sizeof(i_fcolor) * (bxmax - bxmin));
    
    for(y=bymin;y<=bymax;y++) {
      x = bxmin;
      while (x < bxmax) {
        while (x < bxmax && !btm_test(btm, x, y)) {
          ++x;
        }
        if (btm_test(btm, x, y)) {
          start = x;
          while (x < bxmax && btm_test(btm, x, y)) {
            ++x;
          }
          if (fill->combinef)
            i_glinf(im, start, x, y, line);
          (fill->fill_with_fcolor)(fill, start, y, x-start, im->channels, 
                                   line, work);
          i_plinf(im, start, x, y, line);
        }
      }
    }
    myfree(line);
    if (work)
      myfree(work);
  }

  btm_destroy(btm);
}
