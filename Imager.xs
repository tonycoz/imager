#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
#ifdef __cplusplus

#endif

#include "image.h"
#include "feat.h"
#include "dynaload.h"
#include "regmach.h"

typedef io_glue* Imager__IO;
typedef i_color* Imager__Color;
typedef i_img*   Imager__ImgRaw;


#ifdef HAVE_LIBTT
typedef TT_Fonthandle* Imager__TTHandle;
#endif

typedef struct i_reader_data_tag
{
  /* presumably a CODE ref or name of a sub */
  SV *sv;
} i_reader_data;

/* used by functions that want callbacks */
static int read_callback(char *userdata, char *buffer, int need, int want) {
  i_reader_data *rd = (i_reader_data *)userdata;
  int count;
  int result;
  SV *data;
  dSP; dTARG = sv_newmortal();
  /* thanks to Simon Cozens for help with the dTARG above */

  ENTER;
  SAVETMPS;
  EXTEND(SP, 2);
  PUSHMARK(SP);
  PUSHi(want);
  PUSHi(need);
  PUTBACK;

  count = perl_call_sv(rd->sv, G_SCALAR);

  SPAGAIN;

  if (count != 1)
    croak("Result of perl_call_sv(..., G_SCALAR) != 1");

  data = POPs;

  if (SvOK(data)) {
    STRLEN len;
    char *ptr = SvPV(data, len);
    if (len > want)
      croak("Too much data returned in reader callback");
    
    memcpy(buffer, ptr, len);
    result = len;
  }
  else {
    result = -1;
  }

  PUTBACK;
  FREETMPS;
  LEAVE;

  return result;
}

typedef struct
{
  SV *sv; /* a coderef or sub name */
} i_writer_data;

/* used by functions that want callbacks */
static int write_callback(char *userdata, char const *data, int size) {
  i_writer_data *wd = (i_writer_data *)userdata;
  int count;
  int success;
  SV *sv;
  dSP; 

  ENTER;
  SAVETMPS;
  EXTEND(SP, 1);
  PUSHMARK(SP);
  XPUSHs(sv_2mortal(newSVpv((char *)data, size)));
  PUTBACK;

  count = perl_call_sv(wd->sv, G_SCALAR);

  SPAGAIN;

  if (count != 1)
    croak("Result of perl_call_sv(..., G_SCALAR) != 1");

  sv = POPs;
  success = SvTRUE(sv);


  PUTBACK;
  FREETMPS;
  LEAVE;

  return success;
}

struct value_name {
  char *name;
  int value;
};
static int lookup_name(struct value_name *names, int count, char *name, int def_value)
{
  int i;
  for (i = 0; i < count; ++i)
    if (strEQ(names[i].name, name))
      return names[i].value;

  return def_value;
}
static struct value_name transp_names[] =
{
  { "none", tr_none },
  { "threshold", tr_threshold },
  { "errdiff", tr_errdiff },
  { "ordered", tr_ordered, },
};

static struct value_name make_color_names[] =
{
  { "none", mc_none, },
  { "webmap", mc_web_map, },
  { "addi", mc_addi, },
};

static struct value_name translate_names[] =
{
#ifdef HAVE_LIBGIF
  { "giflib", pt_giflib, },
#endif
  { "closest", pt_closest, },
  { "perturb", pt_perturb, },
  { "errdiff", pt_errdiff, },
};

static struct value_name errdiff_names[] =
{
  { "floyd", ed_floyd, },
  { "jarvis", ed_jarvis, },
  { "stucki", ed_stucki, },
  { "custom", ed_custom, },
};

static struct value_name orddith_names[] =
{
  { "random", od_random, },
  { "dot8", od_dot8, },
  { "dot4", od_dot4, },
  { "hline", od_hline, },
  { "vline", od_vline, },
  { "/line", od_slashline, },
  { "slashline", od_slashline, },
  { "\\line", od_backline, },
  { "backline", od_backline, },
  { "tiny", od_tiny, },
  { "custom", od_custom, },
};

/* look through the hash for quantization options */
static void handle_quant_opts(i_quantize *quant, HV *hv)
{
  /*** POSSIBLY BROKEN: do I need to unref the SV from hv_fetch ***/
  SV **sv;
  int i;
  STRLEN len;
  char *str;

  sv = hv_fetch(hv, "transp", 6, 0);
  if (sv && *sv && (str = SvPV(*sv, len))) {
    quant->transp = 
      lookup_name(transp_names, sizeof(transp_names)/sizeof(*transp_names), 
		  str, tr_none);
    if (quant->transp != tr_none) {
      quant->tr_threshold = 127;
      sv = hv_fetch(hv, "tr_threshold", 12, 0);
      if (sv && *sv)
	quant->tr_threshold = SvIV(*sv);
    }
    if (quant->transp == tr_errdiff) {
      sv = hv_fetch(hv, "tr_errdiff", 10, 0);
      if (sv && *sv && (str = SvPV(*sv, len)))
	quant->tr_errdiff = lookup_name(errdiff_names, sizeof(errdiff_names)/sizeof(*errdiff_names), str, ed_floyd);
    }
    if (quant->transp == tr_ordered) {
      quant->tr_orddith = od_tiny;
      sv = hv_fetch(hv, "tr_orddith", 10, 0);
      if (sv && *sv && (str = SvPV(*sv, len)))
	quant->tr_orddith = lookup_name(orddith_names, sizeof(orddith_names)/sizeof(*orddith_names), str, od_random);

      if (quant->tr_orddith == od_custom) {
	sv = hv_fetch(hv, "tr_map", 6, 0);
	if (sv && *sv && SvTYPE(SvRV(*sv)) == SVt_PVAV) {
	  AV *av = (AV*)SvRV(*sv);
	  len = av_len(av) + 1;
	  if (len > sizeof(quant->tr_custom))
	    len = sizeof(quant->tr_custom);
	  for (i = 0; i < len; ++i) {
	    SV **sv2 = av_fetch(av, i, 0);
	    if (sv2 && *sv2) {
	      quant->tr_custom[i] = SvIV(*sv2);
	    }
	  }
	  while (i < sizeof(quant->tr_custom))
	    quant->tr_custom[i++] = 0;
	}
      }
    }
  }
  quant->make_colors = mc_addi;
  sv = hv_fetch(hv, "make_colors", 11, 0);
  if (sv && *sv && (str = SvPV(*sv, len))) {
    quant->make_colors = 
      lookup_name(make_color_names, sizeof(make_color_names)/sizeof(*make_color_names), str, mc_addi);
  }
  sv = hv_fetch(hv, "colors", 6, 0);
  if (sv && *sv && SvROK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVAV) {
    /* needs to be an array of Imager::Color
       note that the caller allocates the mc_color array and sets mc_size
       to it's size */
    AV *av = (AV *)SvRV(*sv);
    quant->mc_count = av_len(av)+1;
    if (quant->mc_count > quant->mc_size)
      quant->mc_count = quant->mc_size;
    for (i = 0; i < quant->mc_count; ++i) {
      SV **sv1 = av_fetch(av, i, 0);
      if (sv1 && *sv1 && SvROK(*sv1) && sv_derived_from(*sv1, "Imager::Color")) {
	i_color *col = (i_color *)SvIV((SV*)SvRV(*sv1));
	quant->mc_colors[i] = *col;
      }
    }
  }
  sv = hv_fetch(hv, "max_colors", 10, 0);
  if (sv && *sv) {
    i = SvIV(*sv);
    if (i <= quant->mc_size && i >= quant->mc_count)
      quant->mc_size = i;
  }

  quant->translate = pt_closest;
  sv = hv_fetch(hv, "translate", 9, 0);
  if (sv && *sv && (str = SvPV(*sv, len))) {
    quant->translate = lookup_name(translate_names, sizeof(translate_names)/sizeof(*translate_names), str, pt_closest);
  }
  sv = hv_fetch(hv, "errdiff", 7, 0);
  if (sv && *sv && (str = SvPV(*sv, len))) {
    quant->errdiff = lookup_name(errdiff_names, sizeof(errdiff_names)/sizeof(*errdiff_names), str, ed_floyd);
  }
  if (quant->translate == pt_errdiff && quant->errdiff == ed_custom) {
    /* get the error diffusion map */
    sv = hv_fetch(hv, "errdiff_width", 13, 0);
    if (sv && *sv)
      quant->ed_width = SvIV(*sv);
    sv = hv_fetch(hv, "errdiff_height", 14, 0);
    if (sv && *sv)
      quant->ed_height = SvIV(*sv);
    sv = hv_fetch(hv, "errdiff_orig", 12, 0);
    if (sv && *sv)
      quant->ed_orig = SvIV(*sv);
    if (quant->ed_width > 0 && quant->ed_height > 0) {
      int sum = 0;
      quant->ed_map = mymalloc(sizeof(int)*quant->ed_width*quant->ed_height);
      sv = hv_fetch(hv, "errdiff_map", 11, 0);
      if (sv && *sv && SvROK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVAV) {
	AV *av = (AV*)SvRV(*sv);
	len = av_len(av) + 1;
	if (len > quant->ed_width * quant->ed_height)
	  len = quant->ed_width * quant->ed_height;
	for (i = 0; i < len; ++i) {
	  SV **sv2 = av_fetch(av, i, 0);
	  if (sv2 && *sv2) {
	    quant->ed_map[i] = SvIV(*sv2);
	    sum += quant->ed_map[i];
	  }
	}
      }
      if (!sum) {
	/* broken map */
	myfree(quant->ed_map);
	quant->ed_map = 0;
	quant->errdiff = ed_floyd;
      }
    }
  }
  sv = hv_fetch(hv, "perturb", 7, 0);
  if (sv && *sv)
    quant->perturb = SvIV(*sv);
}

/* look through the hash for options to add to opts */
static void handle_gif_opts(i_gif_opts *opts, HV *hv)
{
  /*** FIXME: POSSIBLY BROKEN: do I need to unref the SV from hv_fetch? ***/
  SV **sv;
  int i;
  /**((char *)0) = '\0';*/
  sv = hv_fetch(hv, "gif_each_palette", 16, 0);
  if (sv && *sv)
    opts->each_palette = SvIV(*sv);
  sv = hv_fetch(hv, "interlace", 9, 0);
  if (sv && *sv)
    opts->interlace = SvIV(*sv);
  sv = hv_fetch(hv, "gif_delays", 10, 0);
  if (sv && *sv && SvROK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVAV) {
    AV *av = (AV*)SvRV(*sv);
    opts->delay_count = av_len(av)+1;
    opts->delays = mymalloc(sizeof(int) * opts->delay_count);
    for (i = 0; i < opts->delay_count; ++i) {
      SV *sv1 = *av_fetch(av, i, 0);
      opts->delays[i] = SvIV(sv1);
    }
  }
  sv = hv_fetch(hv, "gif_user_input", 14, 0);
  if (sv && *sv && SvROK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVAV) {
    AV *av = (AV*)SvRV(*sv);
    opts->user_input_count = av_len(av)+1;
    opts->user_input_flags = mymalloc(opts->user_input_count);
    for (i = 0; i < opts->user_input_count; ++i) {
      SV *sv1 = *av_fetch(av, i, 0);
      opts->user_input_flags[i] = SvIV(sv1) != 0;
    }
  }
  sv = hv_fetch(hv, "gif_disposal", 12, 0);
  if (sv && *sv && SvROK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVAV) {
    AV *av = (AV*)SvRV(*sv);
    opts->disposal_count = av_len(av)+1;
    opts->disposal = mymalloc(opts->disposal_count);
    for (i = 0; i < opts->disposal_count; ++i) {
      SV *sv1 = *av_fetch(av, i, 0);
      opts->disposal[i] = SvIV(sv1);
    }
  }
  sv = hv_fetch(hv, "gif_tran_color", 14, 0);
  if (sv && *sv && SvROK(*sv) && sv_derived_from(*sv, "Imager::Color")) {
    i_color *col = (i_color *)SvIV((SV *)SvRV(*sv));
    opts->tran_color = *col;
  }
  sv = hv_fetch(hv, "gif_positions", 13, 0);
  if (sv && *sv && SvROK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVAV) {
    AV *av = (AV *)SvRV(*sv);
    opts->position_count = av_len(av) + 1;
    opts->positions = mymalloc(sizeof(i_gif_pos) * opts->position_count);
    for (i = 0; i < opts->position_count; ++i) {
      SV **sv2 = av_fetch(av, i, 0);
      opts->positions[i].x = opts->positions[i].y = 0;
      if (sv && *sv && SvROK(*sv) && SvTYPE(SvRV(*sv)) == SVt_PVAV) {
	AV *av2 = (AV*)SvRV(*sv2);
	SV **sv3;
	sv3 = av_fetch(av2, 0, 0);
	if (sv3 && *sv3)
	  opts->positions[i].x = SvIV(*sv3);
	sv3 = av_fetch(av2, 1, 0);
	if (sv3 && *sv3)
	  opts->positions[i].y = SvIV(*sv3);
      }
    }
  }
  /* Netscape2.0 loop count extension */
  sv = hv_fetch(hv, "gif_loop_count", 14, 0);
  if (sv && *sv)
    opts->loop_count = SvIV(*sv);
}

/* copies the color map from the hv into the colors member of the HV */
static void copy_colors_back(HV *hv, i_quantize *quant) {
  SV **sv;
  AV *av;
  int i;
  SV *work;

  sv = hv_fetch(hv, "colors", 6, 0);
  if (!sv || !*sv || !SvROK(*sv) || SvTYPE(SvRV(*sv)) != SVt_PVAV) {
    SV *ref;
    av = newAV();
    ref = newRV_inc((SV*) av);
    sv = hv_store(hv, "colors", 6, ref, 0);
  }
  else {
    av = (AV *)SvRV(*sv);
  }
  av_extend(av, quant->mc_count+1);
  for (i = 0; i < quant->mc_count; ++i) {
    i_color *in = quant->mc_colors+i;
    Imager__Color c = ICL_new_internal(in->rgb.r, in->rgb.g, in->rgb.b, 255);
    work = sv_newmortal();
    sv_setref_pv(work, "Imager::Color", (void *)c);
    SvREFCNT_inc(work);
    if (!av_store(av, i, work)) {
      SvREFCNT_dec(work);
    }
  }
}

MODULE = Imager		PACKAGE = Imager::Color	PREFIX = ICL_

Imager::Color
ICL_new_internal(r,g,b,a)
               unsigned char     r
               unsigned char     g
               unsigned char     b
               unsigned char     a

void
ICL_DESTROY(cl)
               Imager::Color    cl


void
ICL_set_internal(cl,r,g,b,a)
               Imager::Color    cl
               unsigned char     r
               unsigned char     g
               unsigned char     b
               unsigned char     a
	   PPCODE:
	       ICL_set_internal(cl, r, g, b, a);
	       EXTEND(SP, 1);
	       PUSHs(ST(0));

void
ICL_info(cl)
               Imager::Color    cl


void
ICL_rgba(cl)
	      Imager::Color	cl
	    PPCODE:
		EXTEND(SP, 4);
		PUSHs(sv_2mortal(newSVnv(cl->rgba.r)));
		PUSHs(sv_2mortal(newSVnv(cl->rgba.g)));
		PUSHs(sv_2mortal(newSVnv(cl->rgba.b)));
		PUSHs(sv_2mortal(newSVnv(cl->rgba.a)));







MODULE = Imager		PACKAGE = Imager::ImgRaw	PREFIX = IIM_

Imager::ImgRaw
IIM_new(x,y,ch)
               int     x
	       int     y
	       int     ch

void
IIM_DESTROY(im)
               Imager::ImgRaw    im



MODULE = Imager		PACKAGE = Imager

PROTOTYPES: ENABLE


Imager::IO
io_new_fd(fd)
                         int     fd

Imager::IO
io_new_bufchain()


void
io_slurp(ig)
        Imager::IO     ig
	     PREINIT:
	      unsigned char*    data;
	     size_t    tlength;
                SV*    r;
	     PPCODE:
 	      data    = NULL;
              tlength = io_slurp(ig, &data);
              r = sv_newmortal();
              EXTEND(SP,1);
              PUSHs(sv_2mortal(newSVpv(data,tlength)));
              myfree(data);



void
i_list_formats()
	     PREINIT:
	      char*    item;
	       int     i;
	     PPCODE:
	       i=0;
	       while( (item=i_format_list[i++]) != NULL ) {
		      EXTEND(SP, 1);
		      PUSHs(sv_2mortal(newSVpv(item,0)));
	       }

undef_int
i_has_format(frmt)
              char*    frmt

Imager::ImgRaw
i_img_new()

Imager::ImgRaw
i_img_empty(im,x,y)
    Imager::ImgRaw     im
               int     x
	       int     y

Imager::ImgRaw
i_img_empty_ch(im,x,y,ch)
    Imager::ImgRaw     im
               int     x
	       int     y
	       int     ch

void
init_log(name,level)
	      char*    name
	       int     level

void
i_img_exorcise(im)
    Imager::ImgRaw     im

void
i_img_destroy(im)
    Imager::ImgRaw     im

void
i_img_info(im)
    Imager::ImgRaw     im
	     PREINIT:
	       int     info[4];
	     PPCODE:
   	       i_img_info(im,info);
               EXTEND(SP, 4);
               PUSHs(sv_2mortal(newSViv(info[0])));
               PUSHs(sv_2mortal(newSViv(info[1])));
               PUSHs(sv_2mortal(newSViv(info[2])));
               PUSHs(sv_2mortal(newSViv(info[3])));




void
i_img_setmask(im,ch_mask)
    Imager::ImgRaw     im
	       int     ch_mask

int
i_img_getmask(im)
    Imager::ImgRaw     im

int
i_img_getchannels(im)
    Imager::ImgRaw     im

void
i_img_getdata(im)
    Imager::ImgRaw     im
             PPCODE:
	       EXTEND(SP, 1);
               PUSHs(sv_2mortal(newSVpv(im->data, im->bytes)));


void
i_draw(im,x1,y1,x2,y2,val)
    Imager::ImgRaw     im
	       int     x1
	       int     y1
	       int     x2
	       int     y2
     Imager::Color     val

void
i_line_aa(im,x1,y1,x2,y2,val)
    Imager::ImgRaw     im
	       int     x1
	       int     y1
	       int     x2
	       int     y2
     Imager::Color     val

void
i_box(im,x1,y1,x2,y2,val)
    Imager::ImgRaw     im
	       int     x1
	       int     y1
	       int     x2
	       int     y2
     Imager::Color     val

void
i_box_filled(im,x1,y1,x2,y2,val)
    Imager::ImgRaw     im
	       int     x1
	       int     y1
	       int     x2
	       int     y2
	   Imager::Color    val

void
i_arc(im,x,y,rad,d1,d2,val)
    Imager::ImgRaw     im
	       int     x
	       int     y
             float     rad
             float     d1
             float     d2
	   Imager::Color    val



void
i_bezier_multi(im,xc,yc,val)
    Imager::ImgRaw     im
             Imager::Color  val
	     PREINIT:
	     double   *x,*y;
	     int       len;
	     AV       *av1;
	     AV       *av2;
	     SV       *sv1;
	     SV       *sv2;
	     int i;
	     PPCODE:
	     ICL_info(val);
	     if (!SvROK(ST(1))) croak("Imager: Parameter 1 to i_bezier_multi must be a reference to an array\n");
	     if (SvTYPE(SvRV(ST(1))) != SVt_PVAV) croak("Imager: Parameter 1 to i_bezier_multi must be a reference to an array\n");
	     if (!SvROK(ST(2))) croak("Imager: Parameter 2 to i_bezier_multi must be a reference to an array\n");
	     if (SvTYPE(SvRV(ST(2))) != SVt_PVAV) croak("Imager: Parameter 2 to i_bezier_multi must be a reference to an array\n");
	     av1=(AV*)SvRV(ST(1));
	     av2=(AV*)SvRV(ST(2));
	     if (av_len(av1) != av_len(av2)) croak("Imager: x and y arrays to i_bezier_multi must be equal length\n");
	     len=av_len(av1)+1;
	     x=mymalloc( len*sizeof(double) );
	     y=mymalloc( len*sizeof(double) );
	     for(i=0;i<len;i++) {
	       sv1=(*(av_fetch(av1,i,0)));
	       sv2=(*(av_fetch(av2,i,0)));
	       x[i]=(double)SvNV(sv1);
	       y[i]=(double)SvNV(sv2);
	     }
             i_bezier_multi(im,len,x,y,val);
             myfree(x);
             myfree(y);


void
i_poly_aa(im,xc,yc,val)
    Imager::ImgRaw     im
             Imager::Color  val
	     PREINIT:
	     double   *x,*y;
	     int       len;
	     AV       *av1;
	     AV       *av2;
	     SV       *sv1;
	     SV       *sv2;
	     int i;
	     PPCODE:
	     ICL_info(val);
	     if (!SvROK(ST(1))) croak("Imager: Parameter 1 to i_poly_aa must be a reference to an array\n");
	     if (SvTYPE(SvRV(ST(1))) != SVt_PVAV) croak("Imager: Parameter 1 to i_poly_aa must be a reference to an array\n");
	     if (!SvROK(ST(2))) croak("Imager: Parameter 1 to i_poly_aa must be a reference to an array\n");
	     if (SvTYPE(SvRV(ST(2))) != SVt_PVAV) croak("Imager: Parameter 1 to i_poly_aa must be a reference to an array\n");
	     av1=(AV*)SvRV(ST(1));
	     av2=(AV*)SvRV(ST(2));
	     if (av_len(av1) != av_len(av2)) croak("Imager: x and y arrays to i_poly_aa must be equal length\n");
	     len=av_len(av1)+1;
	     x=mymalloc( len*sizeof(double) );
	     y=mymalloc( len*sizeof(double) );
	     for(i=0;i<len;i++) {
	       sv1=(*(av_fetch(av1,i,0)));
	       sv2=(*(av_fetch(av2,i,0)));
	       x[i]=(double)SvNV(sv1);
	       y[i]=(double)SvNV(sv2);
	     }
             i_poly_aa(im,len,x,y,val);
             myfree(x);
             myfree(y);



void
i_flood_fill(im,seedx,seedy,dcol)
    Imager::ImgRaw     im
	       int     seedx
	       int     seedy
     Imager::Color     dcol


void
i_copyto(im,src,x1,y1,x2,y2,tx,ty)
    Imager::ImgRaw     im
    Imager::ImgRaw     src
	       int     x1
	       int     y1
	       int     x2
	       int     y2
	       int     tx
	       int     ty


void
i_copyto_trans(im,src,x1,y1,x2,y2,tx,ty,trans)
    Imager::ImgRaw     im
    Imager::ImgRaw     src
	       int     x1
	       int     y1
	       int     x2
	       int     y2
	       int     tx
	       int     ty
     Imager::Color     trans

void
i_copy(im,src)
    Imager::ImgRaw     im
    Imager::ImgRaw     src


void
i_rubthru(im,src,tx,ty)
    Imager::ImgRaw     im
    Imager::ImgRaw     src
	       int     tx
	       int     ty

undef_int
i_flipxy(im, direction)
    Imager::ImgRaw     im
	       int     direction


void
i_gaussian(im,stdev)
    Imager::ImgRaw     im
	     float     stdev

void
i_conv(im,pcoef)
    Imager::ImgRaw     im
	     PREINIT:
	     float*    coeff;
	     int     len;
	     AV* av;
	     SV* sv1;
	     int i;
	     PPCODE:
	     if (!SvROK(ST(1))) croak("Imager: Parameter 1 must be a reference to an array\n");
	     if (SvTYPE(SvRV(ST(1))) != SVt_PVAV) croak("Imager: Parameter 1 must be a reference to an array\n");
	     av=(AV*)SvRV(ST(1));
	     len=av_len(av)+1;
	     coeff=mymalloc( len*sizeof(float) );
	     for(i=0;i<len;i++) {
	       sv1=(*(av_fetch(av,i,0)));
	       coeff[i]=(float)SvNV(sv1);
	     }
	     i_conv(im,coeff,len);
	     myfree(coeff);

undef_int
i_convert(im, src, coeff)
    Imager::ImgRaw     im
    Imager::ImgRaw     src
	PREINIT:
    	  float *coeff;
	  int outchan;
	  int inchan;
	  AV *avmain;
          SV **temp;
	  SV *svsub;
          AV *avsub;
	  int len;
	  int i, j;
        CODE:
	  if (!SvROK(ST(2)) || SvTYPE(SvRV(ST(2))) != SVt_PVAV)
	    croak("i_convert: parameter 3 must be an arrayref\n");
          avmain = (AV*)SvRV(ST(2));
	  outchan = av_len(avmain)+1;
          /* find the biggest */
          inchan = 0;
	  for (j=0; j < outchan; ++j) {
	    temp = av_fetch(avmain, j, 0);
	    if (temp && SvROK(*temp) && SvTYPE(SvRV(*temp)) == SVt_PVAV) {
	      avsub = (AV*)SvRV(*temp);
	      len = av_len(avsub)+1;
	      if (len > inchan)
		inchan = len;
	    }
          }
          coeff = mymalloc(sizeof(float) * outchan * inchan);
	  for (j = 0; j < outchan; ++j) {
	    avsub = (AV*)SvRV(*av_fetch(avmain, j, 0));
	    len = av_len(avsub)+1;
	    for (i = 0; i < len; ++i) {
	      temp = av_fetch(avsub, i, 0);
	      if (temp)
		coeff[i+j*inchan] = SvNV(*temp);
	      else
	 	coeff[i+j*inchan] = 0;
	    }
	    while (i < inchan)
	      coeff[i++ + j*inchan] = 0;
	  }
	  RETVAL = i_convert(im, src, coeff, outchan, inchan);
          myfree(coeff);
	OUTPUT:
	  RETVAL


void
i_map(im, pmaps)
    Imager::ImgRaw     im
	PREINIT:
	  unsigned int mask = 0;
	  AV *avmain;
	  AV *avsub;
          SV **temp;
	  int len;
	  int i, j;
	  unsigned char (*maps)[256];
        CODE:
	  if (!SvROK(ST(1)) || SvTYPE(SvRV(ST(1))) != SVt_PVAV)
	    croak("i_map: parameter 2 must be an arrayref\n");
          avmain = (AV*)SvRV(ST(1));
	  len = av_len(avmain)+1;
	  if (im->channels < len) len = im->channels;

	  maps = mymalloc( len * sizeof(unsigned char [256]) );

	  for (j=0; j<len ; j++) {
	    temp = av_fetch(avmain, j, 0);
	    if (temp && SvROK(*temp) && (SvTYPE(SvRV(*temp)) == SVt_PVAV) ) {
	      avsub = (AV*)SvRV(*temp);
	      if(av_len(avsub) != 255) continue;
	      mask |= 1<<j;
              for (i=0; i<256 ; i++) {
		int val;
		temp = av_fetch(avsub, i, 0);
		val = temp ? SvIV(*temp) : 0;
		if (val<0) val = 0;
		if (val>255) val = 255;
		maps[j][i] = val;
	      }
            }
          }
          i_map(im, maps, mask);
	  myfree(maps);



float
i_img_diff(im1,im2)
    Imager::ImgRaw     im1
    Imager::ImgRaw     im2



undef_int	  
i_init_fonts()

#ifdef HAVE_LIBT1

void
i_t1_set_aa(st)
      	       int     st

int
i_t1_new(pfb,afm=NULL)
       	      char*    pfb
       	      char*    afm

int
i_t1_destroy(font_id)
       	       int     font_id


undef_int
i_t1_cp(im,xb,yb,channel,fontnum,points,str,len,align)
    Imager::ImgRaw     im
	       int     xb
	       int     yb
	       int     channel
	       int     fontnum
             float     points
	      char*    str
	       int     len
	       int     align

void
i_t1_bbox(fontnum,point,str,len)
               int     fontnum
	     float     point
	      char*    str
	       int     len
	     PREINIT:
	       int     cords[6];
	     PPCODE:
   	       i_t1_bbox(fontnum,point,str,len,cords);
               EXTEND(SP, 4);
               PUSHs(sv_2mortal(newSViv(cords[0])));
               PUSHs(sv_2mortal(newSViv(cords[1])));
               PUSHs(sv_2mortal(newSViv(cords[2])));
               PUSHs(sv_2mortal(newSViv(cords[3])));
               PUSHs(sv_2mortal(newSViv(cords[4])));
               PUSHs(sv_2mortal(newSViv(cords[5])));



undef_int
i_t1_text(im,xb,yb,cl,fontnum,points,str,len,align)
    Imager::ImgRaw     im
	       int     xb
	       int     yb
     Imager::Color    cl
	       int     fontnum
             float     points
	      char*    str
	       int     len
	       int     align

#endif 

#ifdef HAVE_LIBTT


Imager::TTHandle
i_tt_new(fontname)
	      char*     fontname

void
i_tt_destroy(handle)
     Imager::TTHandle    handle



undef_int
i_tt_text(handle,im,xb,yb,cl,points,str,len,smooth)
  Imager::TTHandle     handle
    Imager::ImgRaw     im
	       int     xb
	       int     yb
     Imager::Color     cl
             float     points
	      char*    str
	       int     len
	       int     smooth


undef_int
i_tt_cp(handle,im,xb,yb,channel,points,str,len,smooth)
  Imager::TTHandle     handle
    Imager::ImgRaw     im
	       int     xb
	       int     yb
	       int     channel
             float     points
	      char*    str
	       int     len
	       int     smooth



undef_int
i_tt_bbox(handle,point,str,len)
  Imager::TTHandle     handle
	     float     point
	      char*    str
	       int     len
	     PREINIT:
	       int     cords[6],rc;
	     PPCODE:
  	       if ((rc=i_tt_bbox(handle,point,str,len,cords))) {
                 EXTEND(SP, 4);
                 PUSHs(sv_2mortal(newSViv(cords[0])));
                 PUSHs(sv_2mortal(newSViv(cords[1])));
                 PUSHs(sv_2mortal(newSViv(cords[2])));
                 PUSHs(sv_2mortal(newSViv(cords[3])));
                 PUSHs(sv_2mortal(newSViv(cords[4])));
                 PUSHs(sv_2mortal(newSViv(cords[5])));
               }


#endif 




#ifdef HAVE_LIBJPEG
undef_int
i_writejpeg(im,fd,qfactor)
    Imager::ImgRaw     im
	       int     fd
	       int     qfactor

void
i_readjpeg(fd)
	       int     fd
	     PREINIT:
	      char*    iptc_itext;
	       int     tlength;
	     i_img*    rimg;
                SV*    r;
	     PPCODE:
 	      iptc_itext = NULL;
	      rimg=i_readjpeg(fd,&iptc_itext,&tlength);
	      if (iptc_itext == NULL) {
		    r = sv_newmortal();
	            EXTEND(SP,1);
	            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
 		    PUSHs(r);
	      } else {
		    r = sv_newmortal();
	            EXTEND(SP,2);
	            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
 		    PUSHs(r);
		    PUSHs(sv_2mortal(newSVpv(iptc_itext,tlength)));
                    myfree(iptc_itext);
	      }


void
i_readjpeg_scalar(...)
          PROTOTYPE: $
            PREINIT:
	      char*    data;
      unsigned int     length;
	      char*    iptc_itext;
	       int     tlength;
	     i_img*    rimg;
                SV*    r;
	     PPCODE:
 	      iptc_itext = NULL;
              data = (char *)SvPV(ST(0), length);
	      rimg=i_readjpeg_scalar(data,length,&iptc_itext,&tlength);
	      mm_log((1,"i_readjpeg_scalar: 0x%08X\n",rimg));
	      if (iptc_itext == NULL) {
		    r = sv_newmortal();
	            EXTEND(SP,1);
	            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
 		    PUSHs(r);
	      } else {
		    r = sv_newmortal();
	            EXTEND(SP,2);
	            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
 		    PUSHs(r);
		    PUSHs(sv_2mortal(newSVpv(iptc_itext,tlength)));
                    myfree(iptc_itext);
	      }


void
i_readjpeg_wiol(ig)
        Imager::IO     ig
	     PREINIT:
	      char*    iptc_itext;
	       int     tlength;
	     i_img*    rimg;
                SV*    r;
	     PPCODE:
 	      iptc_itext = NULL;
	      rimg = i_readjpeg_wiol(ig,-1,&iptc_itext,&tlength);
	      if (iptc_itext == NULL) {
		    r = sv_newmortal();
	            EXTEND(SP,1);
	            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
 		    PUSHs(r);
	      } else {
		    r = sv_newmortal();
	            EXTEND(SP,2);
	            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
 		    PUSHs(r);
		    PUSHs(sv_2mortal(newSVpv(iptc_itext,tlength)));
                    myfree(iptc_itext);
	      }


#endif




#ifdef HAVE_LIBTIFF

Imager::ImgRaw
i_readtiff_wiol(ig, length)
        Imager::IO     ig
	       int     length


undef_int
i_writetiff_wiol(im, ig)
    Imager::ImgRaw     im
        Imager::IO     ig

undef_int
i_writetiff_wiol_faxable(im, ig, fine)
    Imager::ImgRaw     im
        Imager::IO     ig
	       int     fine


#endif /* HAVE_LIBTIFF */





#ifdef HAVE_LIBPNG

Imager::ImgRaw
i_readpng(fd)
	       int     fd


undef_int
i_writepng(im,fd)
    Imager::ImgRaw     im
	       int     fd


Imager::ImgRaw
i_readpng_scalar(...)
          PROTOTYPE: $
            PREINIT:
	      char*    data;
      unsigned int     length;
	       CODE:
              data = (char *)SvPV(ST(0), length);
 	      RETVAL=i_readpng_scalar(data,length);
             OUTPUT:
              RETVAL
	


#endif


#ifdef HAVE_LIBGIF

void
i_giflib_version()
	PPCODE:
	  PUSHs(sv_2mortal(newSVnv(IM_GIFMAJOR+IM_GIFMINOR*0.1)));

undef_int
i_writegif(im,fd,colors,pixdev,fixed)
    Imager::ImgRaw     im
	       int     fd
	       int     colors
               int     pixdev
	     PREINIT:
             int     fixedlen;
	     Imager__Color  fixed;
	     Imager__Color  tmp;
	     AV* av;
	     SV* sv1;
             IV  Itmp;
	     int i;
	     CODE:
	     if (!SvROK(ST(4))) croak("Imager: Parameter 4 must be a reference to an array\n");
	     if (SvTYPE(SvRV(ST(4))) != SVt_PVAV) croak("Imager: Parameter 4 must be a reference to an array\n");
	     av=(AV*)SvRV(ST(4));
	     fixedlen=av_len(av)+1;
	     fixed=mymalloc( fixedlen*sizeof(i_color) );
	     for(i=0;i<fixedlen;i++) {
	       sv1=(*(av_fetch(av,i,0)));
               if (sv_derived_from(sv1, "Imager::Color")) {
                 Itmp = SvIV((SV*)SvRV(sv1));
                 tmp = (i_color*) Itmp;
               } else croak("Imager: one of the elements of array ref is not of Imager::Color type\n");
               fixed[i]=*tmp;
	     }
	     RETVAL=i_writegif(im,fd,colors,pixdev,fixedlen,fixed);
             myfree(fixed);
             ST(0) = sv_newmortal();
             if (RETVAL == 0) ST(0)=&PL_sv_undef;
             else sv_setiv(ST(0), (IV)RETVAL);




undef_int
i_writegifmc(im,fd,colors)
    Imager::ImgRaw     im
	       int     fd
	       int     colors


undef_int
i_writegif_gen(fd, ...)
	       int     fd
      PROTOTYPE: $$@
      PREINIT:
	i_quantize quant;
	i_gif_opts opts;
	i_img **imgs = NULL;
	int img_count;
	int i;
	HV *hv;
      CODE:
	if (items < 3)
	    croak("Usage: i_writegif_gen(fd,hashref, images...)");
	if (!SvROK(ST(1)) || ! SvTYPE(SvRV(ST(1))))
	    croak("i_writegif_gen: Second argument must be a hash ref");
	hv = (HV *)SvRV(ST(1));
	memset(&quant, 0, sizeof(quant));
	quant.mc_size = 256;
	quant.mc_colors = mymalloc(quant.mc_size * sizeof(i_color));
	memset(&opts, 0, sizeof(opts));
	handle_quant_opts(&quant, hv);
	handle_gif_opts(&opts, hv);
	img_count = items - 2;
	RETVAL = 1;
	if (img_count < 1) {
	  RETVAL = 0;
	  i_clear_error();
	  i_push_error(0, "You need to specify images to save");
	}
	else {
          imgs = mymalloc(sizeof(i_img *) * img_count);
          for (i = 0; i < img_count; ++i) {
	    SV *sv = ST(2+i);
	    imgs[i] = NULL;
	    if (SvROK(sv) && sv_derived_from(sv, "Imager::ImgRaw")) {
	      imgs[i] = (i_img *)SvIV((SV*)SvRV(sv));
	    }
	    else {
	      i_clear_error();
	      i_push_error(0, "Only images can be saved");
	      RETVAL = 0;
	      break;
            }
	  }
          if (RETVAL) {
	    RETVAL = i_writegif_gen(&quant, fd, imgs, img_count, &opts);
          }
	  myfree(imgs);
          if (RETVAL) {
	    copy_colors_back(hv, &quant);
          }
	}
             ST(0) = sv_newmortal();
             if (RETVAL == 0) ST(0)=&PL_sv_undef;
             else sv_setiv(ST(0), (IV)RETVAL);

undef_int
i_writegif_callback(cb, maxbuffer,...)
	int maxbuffer;
      PREINIT:
	i_quantize quant;
	i_gif_opts opts;
	i_img **imgs = NULL;
	int img_count;
	int i;
	HV *hv;
        i_writer_data wd;
      CODE:
	if (items < 4)
	    croak("Usage: i_writegif_callback(\\&callback,maxbuffer,hashref, images...)");
	if (!SvROK(ST(2)) || ! SvTYPE(SvRV(ST(2))))
	    croak("i_writegif_callback: Second argument must be a hash ref");
	hv = (HV *)SvRV(ST(2));
	memset(&quant, 0, sizeof(quant));
	quant.mc_size = 256;
	quant.mc_colors = mymalloc(quant.mc_size * sizeof(i_color));
	memset(&opts, 0, sizeof(opts));
	handle_quant_opts(&quant, hv);
	handle_gif_opts(&opts, hv);
	img_count = items - 3;
	RETVAL = 1;
	if (img_count < 1) {
	  RETVAL = 0;
	}
	else {
          imgs = mymalloc(sizeof(i_img *) * img_count);
          for (i = 0; i < img_count; ++i) {
	    SV *sv = ST(3+i);
	    imgs[i] = NULL;
	    if (SvROK(sv) && sv_derived_from(sv, "Imager::ImgRaw")) {
	      imgs[i] = (i_img *)SvIV((SV*)SvRV(sv));
	    }
	    else {
	      RETVAL = 0;
	      break;
            }
	  }
          if (RETVAL) {
	    wd.sv = ST(0);
	    RETVAL = i_writegif_callback(&quant, write_callback, (char *)&wd, maxbuffer, imgs, img_count, &opts);
          }
	  myfree(imgs);
          if (RETVAL) {
	    copy_colors_back(hv, &quant);
          }
	}
             ST(0) = sv_newmortal();
             if (RETVAL == 0) ST(0)=&PL_sv_undef;
             else sv_setiv(ST(0), (IV)RETVAL);

void
i_readgif(fd)
	       int     fd
	      PREINIT:
	        int*    colour_table;
	        int     colours, q, w;
	      i_img*    rimg;
                 SV*    temp[3];
                 AV*    ct; 
                 SV*    r;
	       PPCODE:
 	       colour_table = NULL;
               colours = 0;

	if(GIMME_V == G_ARRAY) {  
            rimg = i_readgif(fd,&colour_table,&colours);
        } else {
            /* don't waste time with colours if they aren't wanted */
            rimg = i_readgif(fd,NULL,NULL);
        }

	if (colour_table == NULL) {
            EXTEND(SP,1);
            r=sv_newmortal();
            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
            PUSHs(r);
	} else {
            /* the following creates an [[r,g,b], [r, g, b], [r, g, b]...] */
            /* I don't know if I have the reference counts right or not :( */
            /* Neither do I :-) */
            /* No Idea here either */

            ct=newAV();
            av_extend(ct, colours);
            for(q=0; q<colours; q++) {
                for(w=0; w<3; w++)
                    temp[w]=sv_2mortal(newSViv(colour_table[q*3 + w]));
                av_store(ct, q, (SV*)newRV_noinc((SV*)av_make(3, temp)));
            }
            myfree(colour_table);
            
            EXTEND(SP,2);
            r=sv_newmortal();
            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
            PUSHs(r);
            PUSHs(newRV_noinc((SV*)ct));
        }





void
i_readgif_scalar(...)
          PROTOTYPE: $
            PREINIT:
               char*    data;
       unsigned int     length;
	        int*    colour_table;
	        int     colours, q, w;
	      i_img*    rimg;
                 SV*    temp[3];
                 AV*    ct; 
                 SV*    r;
	       PPCODE:
        data = (char *)SvPV(ST(0), length);
        colour_table=NULL;
        colours=0;

	if(GIMME_V == G_ARRAY) {  
            rimg=i_readgif_scalar(data,length,&colour_table,&colours);
        } else {
            /* don't waste time with colours if they aren't wanted */
            rimg=i_readgif_scalar(data,length,NULL,NULL);
        }

	if (colour_table == NULL) {
            EXTEND(SP,1);
            r=sv_newmortal();
            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
            PUSHs(r);
	} else {
            /* the following creates an [[r,g,b], [r, g, b], [r, g, b]...] */
            /* I don't know if I have the reference counts right or not :( */
            /* Neither do I :-) */
            ct=newAV();
            av_extend(ct, colours);
            for(q=0; q<colours; q++) {
                for(w=0; w<3; w++)
                    temp[w]=sv_2mortal(newSViv(colour_table[q*3 + w]));
                av_store(ct, q, (SV*)newRV_noinc((SV*)av_make(3, temp)));
            }
            myfree(colour_table);
            
            EXTEND(SP,2);
            r=sv_newmortal();
            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
            PUSHs(r);
            PUSHs(newRV_noinc((SV*)ct));
        }

void
i_readgif_callback(...)
          PROTOTYPE: &
            PREINIT:
               char*    data;
	        int     length;
	        int*    colour_table;
	        int     colours, q, w;
	      i_img*    rimg;
                 SV*    temp[3];
                 AV*    ct; 
                 SV*    r;
       i_reader_data    rd;
	       PPCODE:
	rd.sv = ST(0);
        colour_table=NULL;
        colours=0;

	if(GIMME_V == G_ARRAY) {  
            rimg=i_readgif_callback(read_callback, (char *)&rd,&colour_table,&colours);
        } else {
            /* don't waste time with colours if they aren't wanted */
            rimg=i_readgif_callback(read_callback, (char *)&rd,NULL,NULL);
        }

	if (colour_table == NULL) {
            EXTEND(SP,1);
            r=sv_newmortal();
            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
            PUSHs(r);
	} else {
            /* the following creates an [[r,g,b], [r, g, b], [r, g, b]...] */
            /* I don't know if I have the reference counts right or not :( */
            /* Neither do I :-) */
            /* Neither do I - maybe I'll move this somewhere */
            ct=newAV();
            av_extend(ct, colours);
            for(q=0; q<colours; q++) {
                for(w=0; w<3; w++)
                    temp[w]=sv_2mortal(newSViv(colour_table[q*3 + w]));
                av_store(ct, q, (SV*)newRV_noinc((SV*)av_make(3, temp)));
            }
            myfree(colour_table);
            
            EXTEND(SP,2);
            r=sv_newmortal();
            sv_setref_pv(r, "Imager::ImgRaw", (void*)rimg);
            PUSHs(r);
            PUSHs(newRV_noinc((SV*)ct));
        }






#endif



Imager::ImgRaw
i_readpnm_wiol(ig, length)
        Imager::IO     ig
	       int     length


undef_int
i_writeppm(im,fd)
    Imager::ImgRaw     im
	       int     fd

Imager::ImgRaw
i_readraw(fd,x,y,datachannels,storechannels,intrl)
	       int     fd
	       int     x
	       int     y
	       int     datachannels
	       int     storechannels
	       int     intrl

undef_int
i_writeraw(im,fd)
    Imager::ImgRaw     im
	       int     fd


Imager::ImgRaw
i_scaleaxis(im,Value,Axis)
    Imager::ImgRaw     im
             float     Value
	       int     Axis

Imager::ImgRaw
i_scale_nn(im,scx,scy)
    Imager::ImgRaw     im
             float     scx
             float     scy

Imager::ImgRaw
i_haar(im)
    Imager::ImgRaw     im

int
i_count_colors(im,maxc)
    Imager::ImgRaw     im
               int     maxc


Imager::ImgRaw
i_transform(im,opx,opy,parm)
    Imager::ImgRaw     im
             PREINIT:
             double* parm;
             int*    opx;
             int*    opy;
             int     opxl;
             int     opyl;
             int     parmlen;
             AV* av;
             SV* sv1;
             int i;
             CODE:
             if (!SvROK(ST(1))) croak("Imager: Parameter 1 must be a reference to an array\n");
             if (!SvROK(ST(2))) croak("Imager: Parameter 2 must be a reference to an array\n");
             if (!SvROK(ST(3))) croak("Imager: Parameter 3 must be a reference to an array\n");
             if (SvTYPE(SvRV(ST(1))) != SVt_PVAV) croak("Imager: Parameter 1 must be a reference to an array\n");
             if (SvTYPE(SvRV(ST(2))) != SVt_PVAV) croak("Imager: Parameter 2 must be a reference to an array\n");
             if (SvTYPE(SvRV(ST(3))) != SVt_PVAV) croak("Imager: Parameter 3 must be a reference to an array\n");
             av=(AV*)SvRV(ST(1));
             opxl=av_len(av)+1;
             opx=mymalloc( opxl*sizeof(int) );
             for(i=0;i<opxl;i++) {
               sv1=(*(av_fetch(av,i,0)));
               opx[i]=(int)SvIV(sv1);
             }
             av=(AV*)SvRV(ST(2));
             opyl=av_len(av)+1;
             opy=mymalloc( opyl*sizeof(int) );
             for(i=0;i<opyl;i++) {
               sv1=(*(av_fetch(av,i,0)));
               opy[i]=(int)SvIV(sv1);
             }
             av=(AV*)SvRV(ST(3));
             parmlen=av_len(av)+1;
             parm=mymalloc( parmlen*sizeof(double) );
             for(i=0;i<parmlen;i++) { /* FIXME: Bug? */
               sv1=(*(av_fetch(av,i,0)));
               parm[i]=(double)SvNV(sv1);
             }
             RETVAL=i_transform(im,opx,opxl,opy,opyl,parm,parmlen);
             myfree(parm);
             myfree(opy);
             myfree(opx);
             ST(0) = sv_newmortal();
             if (RETVAL == 0) ST(0)=&PL_sv_undef;
             else sv_setref_pv(ST(0), "Imager::ImgRaw", (void*)RETVAL);

Imager::ImgRaw
i_transform2(width,height,ops,n_regs,c_regs,in_imgs)
	     PREINIT:
             int width;
             int height;
	     double* parm;
	     struct rm_op *ops;
	     unsigned int ops_len;
	     int ops_count;
             double *n_regs;
             int n_regs_count;
             i_color *c_regs;
	     int c_regs_count;
             int in_imgs_count;
             i_img **in_imgs;
	     AV* av;
	     SV* sv1;
             IV tmp;
	     int i;
             CODE:
	     if (!SvROK(ST(3))) croak("Imager: Parameter 4 must be a reference to an array\n");
	     if (!SvROK(ST(4))) croak("Imager: Parameter 5 must be a reference to an array\n");
	     if (!SvROK(ST(5))) croak("Imager: Parameter 6 must be a reference to an array of images\n");
	     if (SvTYPE(SvRV(ST(3))) != SVt_PVAV) croak("Imager: Parameter 4 must be a reference to an array\n");
	     if (SvTYPE(SvRV(ST(4))) != SVt_PVAV) croak("Imager: Parameter 5 must be a reference to an array\n");

	/*if (SvTYPE(SvRV(ST(5))) != SVt_PVAV) croak("Imager: Parameter 6 must be a reference to an array\n");*/

             if (SvTYPE(SvRV(ST(5))) == SVt_PVAV) {
	       av = (AV*)SvRV(ST(5));
               in_imgs_count = av_len(av)+1;
	       for (i = 0; i < in_imgs_count; ++i) {
		 sv1 = *av_fetch(av, i, 0);
		 if (!sv_derived_from(sv1, "Imager::ImgRaw")) {
		   croak("Parameter 5 must contain only images");
		 }
	       }
	     }
	     else {
	       in_imgs_count = 0;
             }
             if (SvTYPE(SvRV(ST(5))) == SVt_PVAV) {
               av = (AV*)SvRV(ST(5));
               in_imgs = mymalloc(in_imgs_count*sizeof(i_img*));
               for (i = 0; i < in_imgs_count; ++i) {              
	         sv1 = *av_fetch(av,i,0);
	         if (!sv_derived_from(sv1, "Imager::ImgRaw")) {
		   croak("Parameter 5 must contain only images");
	         }
                 tmp = SvIV((SV*)SvRV(sv1));
	         in_imgs[i] = (i_img*)tmp;
	       }
	     }
             else {
	       /* no input images */
	       in_imgs = NULL;
             }
             /* default the output size from the first input if possible */
             if (SvOK(ST(0)))
	       width = SvIV(ST(0));
             else if (in_imgs_count)
	       width = in_imgs[0]->xsize;
             else
	       croak("No output image width supplied");

             if (SvOK(ST(1)))
	       height = SvIV(ST(1));
             else if (in_imgs_count)
	       height = in_imgs[0]->ysize;
             else
	       croak("No output image height supplied");

	     ops = (struct rm_op *)SvPV(ST(2), ops_len);
             if (ops_len % sizeof(struct rm_op))
	         croak("Imager: Parameter 3 must be a bitmap of regops\n");
	     ops_count = ops_len / sizeof(struct rm_op);
	     av = (AV*)SvRV(ST(3));
	     n_regs_count = av_len(av)+1;
             n_regs = mymalloc(n_regs_count * sizeof(double));
	     for (i = 0; i < n_regs_count; ++i) {
	       sv1 = *av_fetch(av,i,0);
	       if (SvOK(sv1))
	         n_regs[i] = SvNV(sv1);
	     }
             av = (AV*)SvRV(ST(4));
             c_regs_count = av_len(av)+1;
             c_regs = mymalloc(c_regs_count * sizeof(i_color));
             /* I don't bother initializing the colou?r registers */

	     RETVAL=i_transform2(width, height, 3, ops, ops_count, 
				 n_regs, n_regs_count, 
				 c_regs, c_regs_count, in_imgs, in_imgs_count);
	     if (in_imgs)
	         myfree(in_imgs);
             myfree(n_regs);
	     myfree(c_regs);
             ST(0) = sv_newmortal();
             if (RETVAL == 0) ST(0)=&PL_sv_undef;
             else sv_setref_pv(ST(0), "Imager::ImgRaw", (void*)RETVAL);


void
i_contrast(im,intensity)
    Imager::ImgRaw     im
             float     intensity

void
i_hardinvert(im)
    Imager::ImgRaw     im

void
i_noise(im,amount,type)
    Imager::ImgRaw     im
             float     amount
     unsigned char     type

void
i_bumpmap(im,bump,channel,light_x,light_y,strength)
    Imager::ImgRaw     im
    Imager::ImgRaw     bump
               int     channel
               int     light_x
               int     light_y
               int     strength

void
i_postlevels(im,levels)
    Imager::ImgRaw     im
             int       levels

void
i_mosaic(im,size)
    Imager::ImgRaw     im
               int     size

void
i_watermark(im,wmark,tx,ty,pixdiff)
    Imager::ImgRaw     im
    Imager::ImgRaw     wmark
               int     tx
               int     ty
               int     pixdiff


void
i_autolevels(im,lsat,usat,skew)
    Imager::ImgRaw     im
             float     lsat
             float     usat
             float     skew

void
i_radnoise(im,xo,yo,rscale,ascale)
    Imager::ImgRaw     im
             float     xo
             float     yo
             float     rscale
             float     ascale

void
i_turbnoise(im, xo, yo, scale)
    Imager::ImgRaw     im
             float     xo
             float     yo
             float     scale


void
i_gradgen(im, ...)
    Imager::ImgRaw     im
      PREINIT:
	int num;
	int *xo;
	int *yo;
        i_color *ival;
	int dmeasure;
	int i;
	SV *sv;
	AV *axx;
	AV *ayy;
	AV *ac;
      CODE:
	if (items != 5)
	    croak("Usage: i_gradgen(im, xo, yo, ival, dmeasure)");
	if (!SvROK(ST(1)) || ! SvTYPE(SvRV(ST(1))))
	    croak("i_gradgen: Second argument must be an array ref");
	if (!SvROK(ST(2)) || ! SvTYPE(SvRV(ST(2))))
	    croak("i_gradgen: Third argument must be an array ref");
	if (!SvROK(ST(3)) || ! SvTYPE(SvRV(ST(3))))
	    croak("i_gradgen: Fourth argument must be an array ref");
	axx = (AV *)SvRV(ST(1));
	ayy = (AV *)SvRV(ST(2));
	ac  = (AV *)SvRV(ST(3));
	dmeasure = (int)SvIV(ST(4));
	
        num = av_len(axx) < av_len(ayy) ? av_len(axx) : av_len(ayy);
	num = num <= av_len(ac) ? num : av_len(ac);
	num++; 
	if (num < 2) croak("Usage: i_gradgen array refs must have more than 1 entry each");
	xo = mymalloc( sizeof(int) * num );
	yo = mymalloc( sizeof(int) * num );
	ival = mymalloc( sizeof(i_color) * num );
	for(i = 0; i<num; i++) {
	  xo[i]   = (int)SvIV(* av_fetch(axx, i, 0));
	  yo[i]   = (int)SvIV(* av_fetch(ayy, i, 0));
          sv = *av_fetch(ac, i, 0);
	  if ( !sv_derived_from(sv, "Imager::Color") ) {
	    free(axx); free(ayy); free(ac);
            croak("i_gradgen: Element of fourth argument is not derived from Imager::Color");
	  }
	  ival[i] = *(i_color *)SvIV((SV *)SvRV(sv));
	}
        i_gradgen(im, num, xo, yo, ival, dmeasure);





void
i_errors()
      PREINIT:
        i_errmsg *errors;
	int i;
	int count;
	AV *av;
	SV *ref;
	SV *sv;
      PPCODE:
	errors = i_errors();
	i = 0;
	while (errors[i].msg) {
	  av = newAV();
	  sv = newSVpv(errors[i].msg, strlen(errors[i].msg));
	  if (!av_store(av, 0, sv)) {
	    SvREFCNT_dec(sv);
	  }
	  sv = newSViv(errors[i].code);
	  if (!av_store(av, 1, sv)) {
	    SvREFCNT_dec(sv);
	  }
	  PUSHs(sv_2mortal(newRV_noinc((SV*)av)));
	  ++i;
	}

void
i_nearest_color(im, ...)
    Imager::ImgRaw     im
      PREINIT:
	int num;
	int *xo;
	int *yo;
        i_color *ival;
	int dmeasure;
	int i;
	SV *sv;
	AV *axx;
	AV *ayy;
	AV *ac;
      CODE:
	if (items != 5)
	    croak("Usage: i_nearest_color(im, xo, yo, ival, dmeasure)");
	if (!SvROK(ST(1)) || ! SvTYPE(SvRV(ST(1))))
	    croak("i_nearest_color: Second argument must be an array ref");
	if (!SvROK(ST(2)) || ! SvTYPE(SvRV(ST(2))))
	    croak("i_nearest_color: Third argument must be an array ref");
	if (!SvROK(ST(3)) || ! SvTYPE(SvRV(ST(3))))
	    croak("i_nearest_color: Fourth argument must be an array ref");
	axx = (AV *)SvRV(ST(1));
	ayy = (AV *)SvRV(ST(2));
	ac  = (AV *)SvRV(ST(3));
	dmeasure = (int)SvIV(ST(4));
	
        num = av_len(axx) < av_len(ayy) ? av_len(axx) : av_len(ayy);
	num = num <= av_len(ac) ? num : av_len(ac);
	num++; 
	if (num < 2) croak("Usage: i_nearest_color array refs must have more than 1 entry each");
	xo = mymalloc( sizeof(int) * num );
	yo = mymalloc( sizeof(int) * num );
	ival = mymalloc( sizeof(i_color) * num );
	for(i = 0; i<num; i++) {
	  xo[i]   = (int)SvIV(* av_fetch(axx, i, 0));
	  yo[i]   = (int)SvIV(* av_fetch(ayy, i, 0));
          sv = *av_fetch(ac, i, 0);
	  if ( !sv_derived_from(sv, "Imager::Color") ) {
	    free(axx); free(ayy); free(ac);
            croak("i_nearest_color: Element of fourth argument is not derived from Imager::Color");
	  }
	  ival[i] = *(i_color *)SvIV((SV *)SvRV(sv));
	}
        i_nearest_color(im, num, xo, yo, ival, dmeasure);




void
malloc_state()

void
hashinfo(hv)
	     PREINIT:
	       HV* hv;
	       int stuff;
	     PPCODE:
	       if (!SvROK(ST(0))) croak("Imager: Parameter 0 must be a reference to a hash\n");	       
	       hv=(HV*)SvRV(ST(0));
	       if (SvTYPE(hv)!=SVt_PVHV) croak("Imager: Parameter 0 must be a reference to a hash\n");
	       if (getint(hv,"stuff",&stuff)) printf("ok: %d\n",stuff); else printf("key doesn't exist\n");
	       if (getint(hv,"stuff2",&stuff)) printf("ok: %d\n",stuff); else printf("key doesn't exist\n");
	       
void
DSO_open(filename)
             char*       filename
	     PREINIT:
	       void *rc;
	       char *evstr;
	     PPCODE:
	       rc=DSO_open(filename,&evstr);
               if (rc!=NULL) {
                 if (evstr!=NULL) {
                   EXTEND(SP,2); 
                   PUSHs(sv_2mortal(newSViv((IV)rc)));
                   PUSHs(sv_2mortal(newSVpvn(evstr, strlen(evstr))));
                 } else {
                   EXTEND(SP,1);
                   PUSHs(sv_2mortal(newSViv((IV)rc)));
                 }
               }


undef_int
DSO_close(dso_handle)
             void*       dso_handle

void
DSO_funclist(dso_handle_v)
             void*       dso_handle_v
	     PREINIT:
	       int i;
	       DSO_handle *dso_handle;
	     PPCODE:
	       dso_handle=(DSO_handle*)dso_handle_v;
	       i=0;
	       while( dso_handle->function_list[i].name != NULL) {
	         EXTEND(SP,1);
		 PUSHs(sv_2mortal(newSVpv(dso_handle->function_list[i].name,0)));
	         EXTEND(SP,1);
		 PUSHs(sv_2mortal(newSVpv(dso_handle->function_list[i++].pcode,0)));
	       }


void
DSO_call(handle,func_index,hv)
	       void*  handle
	       int    func_index
	     PREINIT:
	       HV* hv;
	     PPCODE:
	       if (!SvROK(ST(2))) croak("Imager: Parameter 2 must be a reference to a hash\n");	       
	       hv=(HV*)SvRV(ST(2));
	       if (SvTYPE(hv)!=SVt_PVHV) croak("Imager: Parameter 2 must be a reference to a hash\n");
	       DSO_call( (DSO_handle *)handle,func_index,hv);



# this is mostly for testing...
Imager::Color
i_get_pixel(im, x, y)
	Imager::ImgRaw im
	int x
	int y;
      CODE:
	RETVAL = (i_color *)mymalloc(sizeof(i_color));
	i_gpix(im, x, y, RETVAL);
      OUTPUT:
	RETVAL

