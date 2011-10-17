#define PERL_NO_GET_CONTEXT
#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#define NEED_newRV_noinc
#define NEED_sv_2pv_nolen
#define NEED_sv_2pvbyte
#include "ppport.h"
#ifdef __cplusplus
}
#endif

#define i_int_hlines_testing() 1

#include "imager.h"
#include "feat.h"
#include "dynaload.h"
#include "regmach.h"
#include "imextdef.h"
#include "imextpltypes.h"
#include <float.h>

#if i_int_hlines_testing()
#include "imageri.h"
#endif

#include "imperl.h"

/*

Allocate memory that will be discarded when mortals are discarded.

*/

static void *
malloc_temp(pTHX_ size_t size) {
  SV *sv = sv_2mortal(newSV(size));

  return SvPVX(sv);
}

/* These functions are all shared - then comes platform dependant code */
static int getstr(void *hv_t,char *key,char **store) {
  dTHX;
  SV** svpp;
  HV* hv=(HV*)hv_t;

  mm_log((1,"getstr(hv_t %p, key %s, store %p)\n",hv_t,key,store));

  if ( !hv_exists(hv,key,strlen(key)) ) return 0;

  svpp=hv_fetch(hv, key, strlen(key), 0);
  *store=SvPV(*svpp, PL_na );

  return 1;
}

static int getint(void *hv_t,char *key,int *store) {
  dTHX;
  SV** svpp;
  HV* hv=(HV*)hv_t;  

  mm_log((1,"getint(hv_t %p, key %s, store %p)\n",hv_t,key,store));

  if ( !hv_exists(hv,key,strlen(key)) ) return 0;

  svpp=hv_fetch(hv, key, strlen(key), 0);
  *store=(int)SvIV(*svpp);
  return 1;
}

static int getdouble(void *hv_t,char* key,double *store) {
  dTHX;
  SV** svpp;
  HV* hv=(HV*)hv_t;

  mm_log((1,"getdouble(hv_t %p, key %s, store %p)\n",hv_t,key,store));

  if ( !hv_exists(hv,key,strlen(key)) ) return 0;
  svpp=hv_fetch(hv, key, strlen(key), 0);
  *store=(double)SvNV(*svpp);
  return 1;
}

static int getvoid(void *hv_t,char* key,void **store) {
  dTHX;
  SV** svpp;
  HV* hv=(HV*)hv_t;

  mm_log((1,"getvoid(hv_t %p, key %s, store %p)\n",hv_t,key,store));

  if ( !hv_exists(hv,key,strlen(key)) ) return 0;

  svpp=hv_fetch(hv, key, strlen(key), 0);
  *store = INT2PTR(void*, SvIV(*svpp));

  return 1;
}

static int getobj(void *hv_t,char *key,char *type,void **store) {
  dTHX;
  SV** svpp;
  HV* hv=(HV*)hv_t;

  mm_log((1,"getobj(hv_t %p, key %s,type %s, store %p)\n",hv_t,key,type,store));

  if ( !hv_exists(hv,key,strlen(key)) ) return 0;

  svpp=hv_fetch(hv, key, strlen(key), 0);

  if (sv_derived_from(*svpp,type)) {
    IV tmp = SvIV((SV*)SvRV(*svpp));
    *store = INT2PTR(void*, tmp);
  } else {
    mm_log((1,"getobj: key exists in hash but is not of correct type"));
    return 0;
  }

  return 1;
}

UTIL_table_t i_UTIL_table={getstr,getint,getdouble,getvoid,getobj};

void my_SvREFCNT_dec(void *p) {
  dTHX;
  SvREFCNT_dec((SV*)p);
}


static void
i_log_entry(char *string, int level) {
  mm_log((level, "%s", string));
}


#define CBDATA_BUFSIZE 8192

struct cbdata {
  /* the SVs we use to call back to Perl */
  SV *writecb;
  SV *readcb;
  SV *seekcb;
  SV *closecb;
};

static ssize_t
call_reader(struct cbdata *cbd, void *buf, size_t size, 
            size_t maxread) {
  dTHX;
  int count;
  int result;
  SV *data;
  dSP;

  if (!SvOK(cbd->readcb))
    return -1;

  ENTER;
  SAVETMPS;
  EXTEND(SP, 2);
  PUSHMARK(SP);
  PUSHs(sv_2mortal(newSViv(size)));
  PUSHs(sv_2mortal(newSViv(maxread)));
  PUTBACK;

  count = perl_call_sv(cbd->readcb, G_SCALAR);

  SPAGAIN;

  if (count != 1)
    croak("Result of perl_call_sv(..., G_SCALAR) != 1");

  data = POPs;

  if (SvOK(data)) {
    STRLEN len;
    char *ptr = SvPVbyte(data, len);
    if (len > maxread)
      croak("Too much data returned in reader callback (wanted %d, got %d, expected %d)",
      (int)size, (int)len, (int)maxread);
    
    memcpy(buf, ptr, len);
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

static off_t
io_seeker(void *p, off_t offset, int whence) {
  dTHX;
  struct cbdata *cbd = p;
  int count;
  off_t result;
  dSP;

  if (!SvOK(cbd->seekcb))
    return -1;

  ENTER;
  SAVETMPS;
  EXTEND(SP, 2);
  PUSHMARK(SP);
  PUSHs(sv_2mortal(newSViv(offset)));
  PUSHs(sv_2mortal(newSViv(whence)));
  PUTBACK;

  count = perl_call_sv(cbd->seekcb, G_SCALAR);

  SPAGAIN;

  if (count != 1)
    croak("Result of perl_call_sv(..., G_SCALAR) != 1");

  result = POPi;

  PUTBACK;
  FREETMPS;
  LEAVE;

  return result;
}

static ssize_t
io_writer(void *p, void const *data, size_t size) {
  dTHX;
  struct cbdata *cbd = p;
  I32 count;
  SV *sv;
  dSP;
  bool success;

  if (!SvOK(cbd->writecb))
    return -1;

  ENTER;
  SAVETMPS;
  EXTEND(SP, 1);
  PUSHMARK(SP);
  PUSHs(sv_2mortal(newSVpv((char *)data, size)));
  PUTBACK;

  count = perl_call_sv(cbd->writecb, G_SCALAR);

  SPAGAIN;
  if (count != 1)
    croak("Result of perl_call_sv(..., G_SCALAR) != 1");

  sv = POPs;
  success = SvTRUE(sv);


  PUTBACK;
  FREETMPS;
  LEAVE;

  return success ? size : -1;
}

static ssize_t 
io_reader(void *p, void *data, size_t size) {
  struct cbdata *cbd = p;

  return call_reader(cbd, data, size, size);
}

static int io_closer(void *p) {
  dTHX;
  struct cbdata *cbd = p;
  int success = 1;

  if (SvOK(cbd->closecb)) {
    dSP;
    I32 count;
    SV *sv;

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    PUTBACK;

    count = perl_call_sv(cbd->closecb, G_SCALAR);

    SPAGAIN;
    
    sv = POPs;
    success = SvTRUE(sv);

    PUTBACK;
    FREETMPS;
    LEAVE;
  }

  return success ? 0 : -1;
}

static void io_destroyer(void *p) {
  dTHX;
  struct cbdata *cbd = p;

  SvREFCNT_dec(cbd->writecb);
  SvREFCNT_dec(cbd->readcb);
  SvREFCNT_dec(cbd->seekcb);
  SvREFCNT_dec(cbd->closecb);
  myfree(cbd);
}

static i_io_glue_t *
do_io_new_buffer(pTHX_ SV *data_sv) {
  const char *data;
  STRLEN length;

  data = SvPVbyte(data_sv, length);
  SvREFCNT_inc(data_sv);
  return io_new_buffer(data, length, my_SvREFCNT_dec, data_sv);
}

static i_io_glue_t *
do_io_new_cb(pTHX_ SV *writecb, SV *readcb, SV *seekcb, SV *closecb) {
  struct cbdata *cbd;

  cbd = mymalloc(sizeof(struct cbdata));
  cbd->writecb = newSVsv(writecb);
  cbd->readcb = newSVsv(readcb);
  cbd->seekcb = newSVsv(seekcb);
  cbd->closecb = newSVsv(closecb);

  return io_new_cb(cbd, io_reader, io_writer, io_seeker, io_closer, 
		   io_destroyer);
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
  { "mediancut", mc_median_cut, },
  { "mono", mc_mono, },
  { "monochrome", mc_mono, },
};

static struct value_name translate_names[] =
{
  { "giflib", pt_giflib, },
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
static void
ip_handle_quant_opts(pTHX_ i_quantize *quant, HV *hv)
{
  /*** POSSIBLY BROKEN: do I need to unref the SV from hv_fetch ***/
  SV **sv;
  int i;
  STRLEN len;
  char *str;

  quant->mc_colors = mymalloc(quant->mc_size * sizeof(i_color));

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
  quant->make_colors = mc_median_cut;
  sv = hv_fetch(hv, "make_colors", 11, 0);
  if (sv && *sv && (str = SvPV(*sv, len))) {
    quant->make_colors = 
      lookup_name(make_color_names, sizeof(make_color_names)/sizeof(*make_color_names), str, mc_median_cut);
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
	i_color *col = INT2PTR(i_color *, SvIV((SV*)SvRV(*sv1)));
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

static void
ip_cleanup_quant_opts(pTHX_ i_quantize *quant) {
  myfree(quant->mc_colors);
  if (quant->ed_map)
    myfree(quant->ed_map);
}

/* copies the color map from the hv into the colors member of the HV */
static void
ip_copy_colors_back(pTHX_ HV *hv, i_quantize *quant) {
  SV **sv;
  AV *av;
  int i;
  SV *work;

  sv = hv_fetch(hv, "colors", 6, 0);
  if (!sv || !*sv || !SvROK(*sv) || SvTYPE(SvRV(*sv)) != SVt_PVAV) {
    /* nothing to do */
    return;
  }

  av = (AV *)SvRV(*sv);
  av_clear(av);
  av_extend(av, quant->mc_count+1);
  for (i = 0; i < quant->mc_count; ++i) {
    i_color *in = quant->mc_colors+i;
    Imager__Color c = ICL_new_internal(in->rgb.r, in->rgb.g, in->rgb.b, 255);
    work = sv_newmortal();
    sv_setref_pv(work, "Imager::Color", (void *)c);
    SvREFCNT_inc(work);
    av_push(av, work);
  }
}

/* loads the segments of a fountain fill into an array */
static i_fountain_seg *
load_fount_segs(pTHX_ AV *asegs, int *count) {
  /* Each element of segs must contain:
     [ start, middle, end, c0, c1, segtype, colortrans ]
     start, middle, end are doubles from 0 to 1
     c0, c1 are Imager::Color::Float or Imager::Color objects
     segtype, colortrans are ints
  */
  int i, j;
  AV *aseg;
  i_fountain_seg *segs;
  double work[3];
  int worki[2];

  *count = av_len(asegs)+1;
  if (*count < 1) 
    croak("i_fountain must have at least one segment");
  segs = mymalloc(sizeof(i_fountain_seg) * *count);
  for(i = 0; i < *count; i++) {
    SV **sv1 = av_fetch(asegs, i, 0);
    if (!sv1 || !*sv1 || !SvROK(*sv1) 
        || SvTYPE(SvRV(*sv1)) != SVt_PVAV) {
      myfree(segs);
      croak("i_fountain: segs must be an arrayref of arrayrefs");
    }
    aseg = (AV *)SvRV(*sv1);
    if (av_len(aseg) != 7-1) {
      myfree(segs);
      croak("i_fountain: a segment must have 7 members");
    }
    for (j = 0; j < 3; ++j) {
      SV **sv2 = av_fetch(aseg, j, 0);
      if (!sv2 || !*sv2) {
        myfree(segs);
        croak("i_fountain: XS error");
      }
      work[j] = SvNV(*sv2);
    }
    segs[i].start  = work[0];
    segs[i].middle = work[1];
    segs[i].end    = work[2];
    for (j = 0; j < 2; ++j) {
      SV **sv3 = av_fetch(aseg, 3+j, 0);
      if (!sv3 || !*sv3 || !SvROK(*sv3) ||
          (!sv_derived_from(*sv3, "Imager::Color")
           && !sv_derived_from(*sv3, "Imager::Color::Float"))) {
        myfree(segs);
        croak("i_fountain: segs must contain colors in elements 3 and 4");
      }
      if (sv_derived_from(*sv3, "Imager::Color::Float")) {
        segs[i].c[j] = *INT2PTR(i_fcolor *, SvIV((SV *)SvRV(*sv3)));
      }
      else {
        i_color c = *INT2PTR(i_color *, SvIV((SV *)SvRV(*sv3)));
        int ch;
        for (ch = 0; ch < MAXCHANNELS; ++ch) {
          segs[i].c[j].channel[ch] = c.channel[ch] / 255.0;
        }
      }
    }
    for (j = 0; j < 2; ++j) {
      SV **sv2 = av_fetch(aseg, j+5, 0);
      if (!sv2 || !*sv2) {
        myfree(segs);
        croak("i_fountain: XS error");
      }
      worki[j] = SvIV(*sv2);
    }
    segs[i].type = worki[0];
    segs[i].color = worki[1];
  }

  return segs;
}

/* validates the indexes supplied to i_ppal

i_ppal() doesn't do that for speed, but I'm not comfortable doing that
for calls from perl.

*/
static void
validate_i_ppal(i_img *im, i_palidx const *indexes, int count) {
  int color_count = i_colorcount(im);
  int i;

  if (color_count == -1)
    croak("i_plin() called on direct color image");
  
  for (i = 0; i < count; ++i) {
    if (indexes[i] >= color_count) {
      croak("i_plin() called with out of range color index %d (max %d)",
        indexes[i], color_count-1);
    }
  }
}


/* I don't think ICLF_* names belong at the C interface
   this makes the XS code think we have them, to let us avoid 
   putting function bodies in the XS code
*/
#define ICLF_new_internal(r, g, b, a) i_fcolor_new((r), (g), (b), (a))
#define ICLF_DESTROY(cl) i_fcolor_destroy(cl)

#ifdef IMAGER_LOG
#define i_log_enabled() 1
#else
#define i_log_enabled() 0
#endif

#if i_int_hlines_testing()

typedef i_int_hlines *Imager__Internal__Hlines;

static i_int_hlines *
i_int_hlines_new(i_img_dim start_y, i_img_dim count_y, i_img_dim start_x, i_img_dim count_x) {
  i_int_hlines *result = mymalloc(sizeof(i_int_hlines));
  i_int_init_hlines(result, start_y, count_y, start_x, count_x);

  return result;
}

static i_int_hlines *
i_int_hlines_new_img(i_img *im) {
  i_int_hlines *result = mymalloc(sizeof(i_int_hlines));
  i_int_init_hlines_img(result, im);

  return result;
}

static void
i_int_hlines_DESTROY(i_int_hlines *hlines) {
  i_int_hlines_destroy(hlines);
  myfree(hlines);
}

#define i_int_hlines_CLONE_SKIP(cls) 1

static int seg_compare(const void *vleft, const void *vright) {
  const i_int_hline_seg *left = vleft;
  const i_int_hline_seg *right = vright;

  return left->minx - right->minx;
}

static SV *
i_int_hlines_dump(i_int_hlines *hlines) {
  dTHX;
  SV *dump = newSVpvf("start_y: %" i_DF " limit_y: %" i_DF " start_x: %" i_DF " limit_x: %" i_DF"\n",
	i_DFc(hlines->start_y), i_DFc(hlines->limit_y), i_DFc(hlines->start_x), i_DFc(hlines->limit_x));
  i_img_dim y;
  
  for (y = hlines->start_y; y < hlines->limit_y; ++y) {
    i_int_hline_entry *entry = hlines->entries[y-hlines->start_y];
    if (entry) {
      int i;
      /* sort the segments, if any */
      if (entry->count)
        qsort(entry->segs, entry->count, sizeof(i_int_hline_seg), seg_compare);

      sv_catpvf(dump, " %" i_DF " (%" i_DF "):", i_DFc(y), i_DFc(entry->count));
      for (i = 0; i < entry->count; ++i) {
        sv_catpvf(dump, " [%" i_DF ", %" i_DF ")", i_DFc(entry->segs[i].minx), 
                  i_DFc(entry->segs[i].x_limit));
      }
      sv_catpv(dump, "\n");
    }
  }

  return dump;
}

#endif

static off_t
i_sv_off_t(pTHX_ SV *sv) {
#if LSEEKSIZE > IVSIZE
  return (off_t)SvNV(sv);
#else
  return (off_t)SvIV(sv);
#endif
}

static SV *
i_new_sv_off_t(pTHX_ off_t off) {
#if LSEEKSIZE > IVSIZE
  return newSVnv(off);
#else
  return newSViv(off);
#endif
}

static im_pl_ext_funcs im_perl_funcs =
{
  IMAGER_PL_API_VERSION,
  IMAGER_PL_API_LEVEL,
  ip_handle_quant_opts,
  ip_cleanup_quant_opts,
  ip_copy_colors_back
};

#define PERL_PL_SET_GLOBAL_CALLBACKS \
  sv_setiv(get_sv(PERL_PL_FUNCTION_TABLE_NAME, 1), PTR2IV(&im_perl_funcs));

#ifdef IMEXIF_ENABLE
#define i_exif_enabled() 1
#else
#define i_exif_enabled() 0
#endif

/* trying to use more C style names, map them here */
#define i_io_DESTROY(ig) io_glue_destroy(ig)

#define i_img_get_width(im) ((im)->xsize)
#define i_img_get_height(im) ((im)->ysize)

#define i_img_epsilonf() (DBL_EPSILON * 4)

/* avoid some xsubpp strangeness */
#define NEWLINE '\n'

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

Imager::Color
i_hsv_to_rgb(c)
        Imager::Color c
      CODE:
        RETVAL = mymalloc(sizeof(i_color));
        *RETVAL = *c;
        i_hsv_to_rgb(RETVAL);
      OUTPUT:
        RETVAL
        
Imager::Color
i_rgb_to_hsv(c)
        Imager::Color c
      CODE:
        RETVAL = mymalloc(sizeof(i_color));
        *RETVAL = *c;
        i_rgb_to_hsv(RETVAL);
      OUTPUT:
        RETVAL
        


MODULE = Imager        PACKAGE = Imager::Color::Float  PREFIX=ICLF_

Imager::Color::Float
ICLF_new_internal(r, g, b, a)
        double r
        double g
        double b
        double a

void
ICLF_DESTROY(cl)
        Imager::Color::Float    cl

void
ICLF_rgba(cl)
        Imager::Color::Float    cl
      PREINIT:
        int ch;
      PPCODE:
        EXTEND(SP, MAXCHANNELS);
        for (ch = 0; ch < MAXCHANNELS; ++ch) {
        /* printf("%d: %g\n", ch, cl->channel[ch]); */
          PUSHs(sv_2mortal(newSVnv(cl->channel[ch])));
        }

void
ICLF_set_internal(cl,r,g,b,a)
        Imager::Color::Float    cl
        double     r
        double     g
        double     b
        double     a
      PPCODE:
        cl->rgba.r = r;
        cl->rgba.g = g;
        cl->rgba.b = b;
        cl->rgba.a = a;                
        EXTEND(SP, 1);
        PUSHs(ST(0));

Imager::Color::Float
i_hsv_to_rgb(c)
        Imager::Color::Float c
      CODE:
        RETVAL = mymalloc(sizeof(i_fcolor));
        *RETVAL = *c;
        i_hsv_to_rgbf(RETVAL);
      OUTPUT:
        RETVAL
        
Imager::Color::Float
i_rgb_to_hsv(c)
        Imager::Color::Float c
      CODE:
        RETVAL = mymalloc(sizeof(i_fcolor));
        *RETVAL = *c;
        i_rgb_to_hsvf(RETVAL);
      OUTPUT:
        RETVAL

MODULE = Imager		PACKAGE = Imager::ImgRaw	PREFIX = IIM_

Imager::ImgRaw
IIM_new(x,y,ch)
               i_img_dim     x
	       i_img_dim     y
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


Imager::IO
io_new_buffer(data_sv)
	  SV   *data_sv
	CODE:
	  RETVAL = do_io_new_buffer(aTHX_ data_sv);
        OUTPUT:
          RETVAL

Imager::IO
io_new_cb(writecb, readcb, seekcb, closecb, maxwrite = CBDATA_BUFSIZE)
        SV *writecb;
        SV *readcb;
        SV *seekcb;
        SV *closecb;
        int maxwrite;
      CODE:
        RETVAL = do_io_new_cb(aTHX_ writecb, readcb, seekcb, closecb);
      OUTPUT:
        RETVAL

SV *
io_slurp(ig)
        Imager::IO     ig
	     PREINIT:
	      unsigned char*    data;
	      size_t    tlength;
	     CODE:
 	      data    = NULL;
              tlength = io_slurp(ig, &data);
              RETVAL = newSVpv((char *)data,tlength);
              myfree(data);
	     OUTPUT:
	      RETVAL


undef_int
i_set_image_file_limits(width, height, bytes)
	i_img_dim width
	i_img_dim height
	size_t bytes

void
i_get_image_file_limits()
      PREINIT:
        i_img_dim width, height;
	size_t bytes;
      PPCODE:
        if (i_get_image_file_limits(&width, &height, &bytes)) {
	  EXTEND(SP, 3);
          PUSHs(sv_2mortal(newSViv(width)));
          PUSHs(sv_2mortal(newSViv(height)));
          PUSHs(sv_2mortal(newSVuv(bytes)));
        }

MODULE = Imager		PACKAGE = Imager::IO	PREFIX = io_

Imager::IO
io_new_fd(class, fd)
	int fd
    CODE:
	RETVAL = io_new_fd(fd);
    OUTPUT:
	RETVAL

Imager::IO
io_new_buffer(class, data_sv)
	SV *data_sv
    CODE:
        RETVAL = do_io_new_buffer(aTHX_ data_sv);
    OUTPUT:
        RETVAL

Imager::IO
io_new_cb(class, writecb, readcb, seekcb, closecb)
        SV *writecb;
        SV *readcb;
        SV *seekcb;
        SV *closecb;
    CODE:
        RETVAL = do_io_new_cb(aTHX_ writecb, readcb, seekcb, closecb);
    OUTPUT:
        RETVAL

Imager::IO
io_new_bufchain(class)
    CODE:
	RETVAL = io_new_bufchain();
    OUTPUT:
        RETVAL

SV *
io_slurp(class, ig)
        Imager::IO     ig
    PREINIT:
	unsigned char*    data;
	size_t    tlength;
    CODE:
	data    = NULL;
	tlength = io_slurp(ig, &data);
	RETVAL = newSVpv((char *)data,tlength);
	myfree(data);
    OUTPUT:
	RETVAL

MODULE = Imager		PACKAGE = Imager::IO	PREFIX = i_io_

IV
i_io_raw_write(ig, data_sv)
	Imager::IO ig
	SV *data_sv
      PREINIT:
        void *data;
	STRLEN size;
      CODE:
#ifdef SvUTF8
        if (SvUTF8(data_sv)) {
	  data_sv = sv_2mortal(newSVsv(data_sv));
          /* yes, we want this to croak() if the SV can't be downgraded */
	  sv_utf8_downgrade(data_sv, FALSE);
	}
#endif        
	data = SvPV(data_sv, size);
        RETVAL = i_io_raw_write(ig, data, size);
      OUTPUT:
	RETVAL

void
i_io_raw_read(ig, buffer_sv, size)
	Imager::IO ig
	SV *buffer_sv
	IV size
      PREINIT:
        void *buffer;
	ssize_t result;
      PPCODE:
        if (size <= 0)
	  croak("size negative in call to i_io_raw_read()");
        /* prevent an undefined value warning if they supplied an 
	   undef buffer.
           Orginally conditional on !SvOK(), but this will prevent the
	   downgrade from croaking */
	sv_setpvn(buffer_sv, "", 0);
#ifdef SvUTF8
	if (SvUTF8(buffer_sv))
          sv_utf8_downgrade(buffer_sv, FALSE);
#endif
	buffer = SvGROW(buffer_sv, size+1);
        result = i_io_raw_read(ig, buffer, size);
        if (result >= 0) {
	  SvCUR_set(buffer_sv, result);
	  *SvEND(buffer_sv) = '\0';
	  SvPOK_only(buffer_sv);
	  EXTEND(SP, 1);
	  PUSHs(sv_2mortal(newSViv(result)));
	}
	ST(1) = buffer_sv;
	SvSETMAGIC(ST(1));

void
i_io_raw_read2(ig, size)
	Imager::IO ig
	IV size
      PREINIT:
	SV *buffer_sv;
        void *buffer;
	ssize_t result;
      PPCODE:
        if (size <= 0)
	  croak("size negative in call to i_io_read2()");
	buffer_sv = newSV(size);
	buffer = SvGROW(buffer_sv, size+1);
        result = i_io_raw_read(ig, buffer, size);
        if (result >= 0) {
	  SvCUR_set(buffer_sv, result);
	  *SvEND(buffer_sv) = '\0';
	  SvPOK_only(buffer_sv);
	  EXTEND(SP, 1);
	  PUSHs(sv_2mortal(buffer_sv));
	}
	else {
          /* discard it */
	  SvREFCNT_dec(buffer_sv);
        }

off_t
i_io_raw_seek(ig, position, whence)
	Imager::IO ig
	off_t position
	int whence

int
i_io_raw_close(ig)
	Imager::IO ig

void
i_io_DESTROY(ig)
        Imager::IO     ig

int
i_io_CLONE_SKIP(...)
    CODE:
        (void)items; /* avoid unused warning for XS variable */
	RETVAL = 1;
    OUTPUT:
	RETVAL

int
i_io_getc(ig)
	Imager::IO ig

int
i_io_putc(ig, c)
	Imager::IO ig
        int c

int
i_io_close(ig)
	Imager::IO ig

int
i_io_flush(ig)
	Imager::IO ig

int
i_io_peekc(ig)
	Imager::IO ig

int
i_io_seek(ig, off, whence)
	Imager::IO ig
	off_t off
        int whence

void
i_io_peekn(ig, size)
	Imager::IO ig
	STRLEN size
      PREINIT:
	SV *buffer_sv;
        void *buffer;
	ssize_t result;
      PPCODE:
	buffer_sv = newSV(size+1);
	buffer = SvGROW(buffer_sv, size+1);
        result = i_io_peekn(ig, buffer, size);
        if (result >= 0) {
	  SvCUR_set(buffer_sv, result);
	  *SvEND(buffer_sv) = '\0';
	  SvPOK_only(buffer_sv);
	  EXTEND(SP, 1);
	  PUSHs(sv_2mortal(buffer_sv));
	}
	else {
          /* discard it */
	  SvREFCNT_dec(buffer_sv);
        }

void
i_io_read(ig, buffer_sv, size)
	Imager::IO ig
	SV *buffer_sv
	IV size
      PREINIT:
        void *buffer;
	ssize_t result;
      PPCODE:
        if (size <= 0)
	  croak("size negative in call to i_io_read()");
        /* prevent an undefined value warning if they supplied an 
	   undef buffer.
           Orginally conditional on !SvOK(), but this will prevent the
	   downgrade from croaking */
	sv_setpvn(buffer_sv, "", 0);
#ifdef SvUTF8
	if (SvUTF8(buffer_sv))
          sv_utf8_downgrade(buffer_sv, FALSE);
#endif
	buffer = SvGROW(buffer_sv, size+1);
        result = i_io_read(ig, buffer, size);
        if (result >= 0) {
	  SvCUR_set(buffer_sv, result);
	  *SvEND(buffer_sv) = '\0';
	  SvPOK_only(buffer_sv);
	  EXTEND(SP, 1);
	  PUSHs(sv_2mortal(newSViv(result)));
	}
	ST(1) = buffer_sv;
	SvSETMAGIC(ST(1));

void
i_io_read2(ig, size)
	Imager::IO ig
	STRLEN size
      PREINIT:
	SV *buffer_sv;
        void *buffer;
	ssize_t result;
      PPCODE:
        if (size == 0)
	  croak("size zero in call to read2()");
	buffer_sv = newSV(size);
	buffer = SvGROW(buffer_sv, size+1);
        result = i_io_read(ig, buffer, size);
        if (result > 0) {
	  SvCUR_set(buffer_sv, result);
	  *SvEND(buffer_sv) = '\0';
	  SvPOK_only(buffer_sv);
	  EXTEND(SP, 1);
	  PUSHs(sv_2mortal(buffer_sv));
	}
	else {
          /* discard it */
	  SvREFCNT_dec(buffer_sv);
        }

void
i_io_gets(ig, size = 8192, eol = NEWLINE)
	Imager::IO ig
	STRLEN size
	int eol
      PREINIT:
	SV *buffer_sv;
        void *buffer;
	ssize_t result;
      PPCODE:
        if (size < 2)
	  croak("size too small in call to gets()");
	buffer_sv = sv_2mortal(newSV(size+1));
	buffer = SvPVX(buffer_sv);
        result = i_io_gets(ig, buffer, size+1, eol);
        if (result > 0) {
	  SvCUR_set(buffer_sv, result);
	  *SvEND(buffer_sv) = '\0';
	  SvPOK_only(buffer_sv);
	  EXTEND(SP, 1);
	  PUSHs(buffer_sv);
	}

IV
i_io_write(ig, data_sv)
	Imager::IO ig
	SV *data_sv
      PREINIT:
        void *data;
	STRLEN size;
      CODE:
#ifdef SvUTF8
        if (SvUTF8(data_sv)) {
	  data_sv = sv_2mortal(newSVsv(data_sv));
          /* yes, we want this to croak() if the SV can't be downgraded */
	  sv_utf8_downgrade(data_sv, FALSE);
	}
#endif        
	data = SvPV(data_sv, size);
        RETVAL = i_io_write(ig, data, size);
      OUTPUT:
	RETVAL

void
i_io_dump(ig, flags = I_IO_DUMP_DEFAULT)
	Imager::IO ig
	int flags

bool
i_io_set_buffered(ig, flag = 1)
	Imager::IO ig
	int flag

bool
i_io_is_buffered(ig)
	Imager::IO ig

bool
i_io_eof(ig)
	Imager::IO ig

bool
i_io_error(ig)
	Imager::IO ig

MODULE = Imager		PACKAGE = Imager

PROTOTYPES: ENABLE

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

Imager::ImgRaw
i_img_new()

Imager::ImgRaw
i_img_empty(im,x,y)
    Imager::ImgRaw     im
               i_img_dim     x
	       i_img_dim     y

Imager::ImgRaw
i_img_empty_ch(im,x,y,ch)
    Imager::ImgRaw     im
               i_img_dim     x
	       i_img_dim     y
	       int     ch

Imager::ImgRaw
i_sametype(im, x, y)
    Imager::ImgRaw im
               i_img_dim x
               i_img_dim y

Imager::ImgRaw
i_sametype_chans(im, x, y, channels)
    Imager::ImgRaw im
               i_img_dim x
               i_img_dim y
               int channels

int
i_init_log(name_sv,level)
	      SV*    name_sv
	       int     level
	PREINIT:
	  const char *name = SvOK(name_sv) ? SvPV_nolen(name_sv) : NULL;
	CODE:
	  RETVAL = i_init_log(name, level);
	OUTPUT:
	  RETVAL

void
i_log_entry(string,level)
	      char*    string
	       int     level

int
i_log_enabled()

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
	       i_img_dim     info[4];
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
               PUSHs(im->idata ? 
	             sv_2mortal(newSVpv((char *)im->idata, im->bytes)) 
		     : &PL_sv_undef);

IV
i_img_get_width(im)
    Imager::ImgRaw	im

IV
i_img_get_height(im)
    Imager::ImgRaw	im


void
i_img_is_monochrome(im)
	Imager::ImgRaw im
      PREINIT:
	int zero_is_white;
	int result;
      PPCODE:
	result = i_img_is_monochrome(im, &zero_is_white);
	if (result) {
	  if (GIMME_V == G_ARRAY) {
	    EXTEND(SP, 2);
	    PUSHs(&PL_sv_yes);
	    PUSHs(sv_2mortal(newSViv(zero_is_white)));
 	  }
	  else {
	    EXTEND(SP, 1);
	    PUSHs(&PL_sv_yes);
	  }
	}

void
i_line(im,x1,y1,x2,y2,val,endp)
    Imager::ImgRaw     im
	       i_img_dim     x1
	       i_img_dim     y1
	       i_img_dim     x2
	       i_img_dim     y2
     Imager::Color     val
	       int     endp

void
i_line_aa(im,x1,y1,x2,y2,val,endp)
    Imager::ImgRaw     im
	       i_img_dim     x1
	       i_img_dim     y1
	       i_img_dim     x2
	       i_img_dim     y2
     Imager::Color     val
	       int     endp

void
i_box(im,x1,y1,x2,y2,val)
    Imager::ImgRaw     im
	       i_img_dim     x1
	       i_img_dim     y1
	       i_img_dim     x2
	       i_img_dim     y2
     Imager::Color     val

void
i_box_filled(im,x1,y1,x2,y2,val)
    Imager::ImgRaw     im
	       i_img_dim     x1
	       i_img_dim     y1
	       i_img_dim     x2
	       i_img_dim     y2
	   Imager::Color    val

int
i_box_filledf(im,x1,y1,x2,y2,val)
    Imager::ImgRaw     im
	       i_img_dim     x1
	       i_img_dim     y1
	       i_img_dim     x2
	       i_img_dim     y2
	   Imager::Color::Float    val

void
i_box_cfill(im,x1,y1,x2,y2,fill)
    Imager::ImgRaw     im
	       i_img_dim     x1
	       i_img_dim     y1
	       i_img_dim     x2
	       i_img_dim     y2
	   Imager::FillHandle    fill

void
i_arc(im,x,y,rad,d1,d2,val)
    Imager::ImgRaw     im
	       i_img_dim     x
	       i_img_dim     y
             double     rad
             double     d1
             double     d2
	   Imager::Color    val

void
i_arc_aa(im,x,y,rad,d1,d2,val)
    Imager::ImgRaw     im
	    double     x
	    double     y
            double     rad
            double     d1
            double     d2
	   Imager::Color    val

void
i_arc_cfill(im,x,y,rad,d1,d2,fill)
    Imager::ImgRaw     im
	       i_img_dim     x
	       i_img_dim     y
             double     rad
             double     d1
             double     d2
	   Imager::FillHandle    fill

void
i_arc_aa_cfill(im,x,y,rad,d1,d2,fill)
    Imager::ImgRaw     im
	    double     x
	    double     y
            double     rad
            double     d1
            double     d2
	   Imager::FillHandle	fill


void
i_circle_aa(im,x,y,rad,val)
    Imager::ImgRaw     im
	     double     x
	     double     y
             double     rad
	   Imager::Color    val

int
i_circle_out(im,x,y,rad,val)
    Imager::ImgRaw     im
	     i_img_dim     x
	     i_img_dim     y
             i_img_dim     rad
	   Imager::Color    val

int
i_circle_out_aa(im,x,y,rad,val)
    Imager::ImgRaw     im
	     i_img_dim     x
	     i_img_dim     y
             i_img_dim     rad
	   Imager::Color    val

int
i_arc_out(im,x,y,rad,d1,d2,val)
    Imager::ImgRaw     im
	     i_img_dim     x
	     i_img_dim     y
             i_img_dim     rad
	     double d1
	     double d2
	   Imager::Color    val

int
i_arc_out_aa(im,x,y,rad,d1,d2,val)
    Imager::ImgRaw     im
	     i_img_dim     x
	     i_img_dim     y
             i_img_dim     rad
	     double d1
	     double d2
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


int
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
	     CODE:
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
             RETVAL = i_poly_aa(im,len,x,y,val);
             myfree(x);
             myfree(y);
	     OUTPUT:
	       RETVAL

int
i_poly_aa_cfill(im,xc,yc,fill)
    Imager::ImgRaw     im
     Imager::FillHandle     fill
	     PREINIT:
	     double   *x,*y;
	     int       len;
	     AV       *av1;
	     AV       *av2;
	     SV       *sv1;
	     SV       *sv2;
	     int i;
	     CODE:
	     if (!SvROK(ST(1))) croak("Imager: Parameter 1 to i_poly_aa_cfill must be a reference to an array\n");
	     if (SvTYPE(SvRV(ST(1))) != SVt_PVAV) croak("Imager: Parameter 1 to i_poly_aa_cfill must be a reference to an array\n");
	     if (!SvROK(ST(2))) croak("Imager: Parameter 1 to i_poly_aa_cfill must be a reference to an array\n");
	     if (SvTYPE(SvRV(ST(2))) != SVt_PVAV) croak("Imager: Parameter 1 to i_poly_aa_cfill must be a reference to an array\n");
	     av1=(AV*)SvRV(ST(1));
	     av2=(AV*)SvRV(ST(2));
	     if (av_len(av1) != av_len(av2)) croak("Imager: x and y arrays to i_poly_aa_cfill must be equal length\n");
	     len=av_len(av1)+1;
	     x=mymalloc( len*sizeof(double) );
	     y=mymalloc( len*sizeof(double) );
	     for(i=0;i<len;i++) {
	       sv1=(*(av_fetch(av1,i,0)));
	       sv2=(*(av_fetch(av2,i,0)));
	       x[i]=(double)SvNV(sv1);
	       y[i]=(double)SvNV(sv2);
	     }
             RETVAL = i_poly_aa_cfill(im,len,x,y,fill);
             myfree(x);
             myfree(y);
	     OUTPUT:
	       RETVAL



undef_int
i_flood_fill(im,seedx,seedy,dcol)
    Imager::ImgRaw     im
	       i_img_dim     seedx
	       i_img_dim     seedy
     Imager::Color     dcol

undef_int
i_flood_cfill(im,seedx,seedy,fill)
    Imager::ImgRaw     im
	       i_img_dim     seedx
	       i_img_dim     seedy
     Imager::FillHandle     fill

undef_int
i_flood_fill_border(im,seedx,seedy,dcol, border)
    Imager::ImgRaw     im
	       i_img_dim     seedx
	       i_img_dim     seedy
     Imager::Color     dcol
     Imager::Color     border

undef_int
i_flood_cfill_border(im,seedx,seedy,fill, border)
    Imager::ImgRaw     im
	       i_img_dim     seedx
	       i_img_dim     seedy
     Imager::FillHandle     fill
     Imager::Color     border


void
i_copyto(im,src,x1,y1,x2,y2,tx,ty)
    Imager::ImgRaw     im
    Imager::ImgRaw     src
	       i_img_dim     x1
	       i_img_dim     y1
	       i_img_dim     x2
	       i_img_dim     y2
	       i_img_dim     tx
	       i_img_dim     ty


void
i_copyto_trans(im,src,x1,y1,x2,y2,tx,ty,trans)
    Imager::ImgRaw     im
    Imager::ImgRaw     src
	       i_img_dim     x1
	       i_img_dim     y1
	       i_img_dim     x2
	       i_img_dim     y2
	       i_img_dim     tx
	       i_img_dim     ty
     Imager::Color     trans

Imager::ImgRaw
i_copy(src)
    Imager::ImgRaw     src


undef_int
i_rubthru(im,src,tx,ty,src_minx,src_miny,src_maxx,src_maxy)
    Imager::ImgRaw     im
    Imager::ImgRaw     src
	       i_img_dim     tx
	       i_img_dim     ty
	       i_img_dim     src_minx
	       i_img_dim     src_miny
	       i_img_dim     src_maxx
	       i_img_dim     src_maxy

undef_int
i_compose(out, src, out_left, out_top, src_left, src_top, width, height, combine = ic_normal, opacity = 0.0)
    Imager::ImgRaw out
    Imager::ImgRaw src
	i_img_dim out_left
 	i_img_dim out_top
	i_img_dim src_left
	i_img_dim src_top
	i_img_dim width
	i_img_dim height
	int combine
	double opacity

undef_int
i_compose_mask(out, src, mask, out_left, out_top, src_left, src_top, mask_left, mask_top, width, height, combine = ic_normal, opacity = 0.0)
    Imager::ImgRaw out
    Imager::ImgRaw src
    Imager::ImgRaw mask
	i_img_dim out_left
 	i_img_dim out_top
	i_img_dim src_left
	i_img_dim src_top
	i_img_dim mask_left
	i_img_dim mask_top
	i_img_dim width
	i_img_dim height
	int combine
	double opacity

Imager::ImgRaw
i_combine(src_av, channels_av = NULL)
	AV *src_av
	AV *channels_av
  PREINIT:
	i_img **imgs = NULL;
	STRLEN in_count;
	int *channels = NULL;
	int i;
	SV **psv;
	IV tmp;
  CODE:
	in_count = av_len(src_av) + 1;
	if (in_count > 0) {
	  imgs = mymalloc(sizeof(i_img*) * in_count);
	  channels = mymalloc(sizeof(int) * in_count);
	  for (i = 0; i < in_count; ++i) {
	    psv = av_fetch(src_av, i, 0);
	    if (!psv || !*psv || !sv_derived_from(*psv, "Imager::ImgRaw")) {
	      myfree(imgs);
	      myfree(channels);
	      croak("imgs must contain only images");
	    }
	    tmp = SvIV((SV*)SvRV(*psv));
	    imgs[i] = INT2PTR(i_img*, tmp);
	    if (channels_av &&
	        (psv = av_fetch(channels_av, i, 0)) != NULL &&
		*psv) {
	      channels[i] = SvIV(*psv);
	    }
	    else {
	      channels[i] = 0;
	    }
	  }
	}
	RETVAL = i_combine(imgs, channels, in_count);
	myfree(imgs);
	myfree(channels);
  OUTPUT:
	RETVAL

undef_int
i_flipxy(im, direction)
    Imager::ImgRaw     im
	       int     direction

Imager::ImgRaw
i_rotate90(im, degrees)
    Imager::ImgRaw      im
               int      degrees

Imager::ImgRaw
i_rotate_exact(im, amount, ...)
    Imager::ImgRaw      im
            double      amount
      PREINIT:
	i_color *backp = NULL;
	i_fcolor *fbackp = NULL;
	int i;
	SV * sv1;
      CODE:
	/* extract the bg colors if any */
	/* yes, this is kind of strange */
	for (i = 2; i < items; ++i) {
          sv1 = ST(i);
          if (sv_derived_from(sv1, "Imager::Color")) {
	    IV tmp = SvIV((SV*)SvRV(sv1));
	    backp = INT2PTR(i_color *, tmp);
	  }
	  else if (sv_derived_from(sv1, "Imager::Color::Float")) {
	    IV tmp = SvIV((SV*)SvRV(sv1));
	    fbackp = INT2PTR(i_fcolor *, tmp);
	  }
	}
	RETVAL = i_rotate_exact_bg(im, amount, backp, fbackp);
      OUTPUT:
	RETVAL

Imager::ImgRaw
i_matrix_transform(im, xsize, ysize, matrix, ...)
    Imager::ImgRaw      im
               i_img_dim      xsize
               i_img_dim      ysize
      PREINIT:
        double matrix[9];
        AV *av;
        IV len;
        SV *sv1;
        int i;
	i_color *backp = NULL;
	i_fcolor *fbackp = NULL;
      CODE:
        if (!SvROK(ST(3)) || SvTYPE(SvRV(ST(3))) != SVt_PVAV)
          croak("i_matrix_transform: parameter 4 must be an array ref\n");
	av=(AV*)SvRV(ST(3));
	len=av_len(av)+1;
        if (len > 9)
          len = 9;
        for (i = 0; i < len; ++i) {
	  sv1=(*(av_fetch(av,i,0)));
	  matrix[i] = SvNV(sv1);
        }
        for (; i < 9; ++i)
          matrix[i] = 0;
	/* extract the bg colors if any */
	/* yes, this is kind of strange */
	for (i = 4; i < items; ++i) {
          sv1 = ST(i);
          if (sv_derived_from(sv1, "Imager::Color")) {
	    IV tmp = SvIV((SV*)SvRV(sv1));
	    backp = INT2PTR(i_color *, tmp);
	  }
	  else if (sv_derived_from(sv1, "Imager::Color::Float")) {
	    IV tmp = SvIV((SV*)SvRV(sv1));
	    fbackp = INT2PTR(i_fcolor *, tmp);
	  }
	}
        RETVAL = i_matrix_transform_bg(im, xsize, ysize, matrix, backp, fbackp);
      OUTPUT:
        RETVAL

undef_int
i_gaussian(im,stdev)
    Imager::ImgRaw     im
	    double     stdev

void
i_unsharp_mask(im,stdev,scale)
    Imager::ImgRaw     im
	     double    stdev
             double    scale

int
i_conv(im,coef)
	Imager::ImgRaw     im
	AV *coef
     PREINIT:
	double*    c_coef;
	int     len;
	SV* sv1;
	int i;
    CODE:
	len = av_len(coef) + 1;
	c_coef=mymalloc( len * sizeof(double) );
	for(i = 0; i  < len; i++) {
	  sv1 = (*(av_fetch(coef, i, 0)));
	  c_coef[i] = (double)SvNV(sv1);
	}
	RETVAL = i_conv(im, c_coef, len);
	myfree(c_coef);
    OUTPUT:
	RETVAL

Imager::ImgRaw
i_convert(src, avmain)
    Imager::ImgRaw     src
    AV *avmain
	PREINIT:
    	  double *coeff;
	  int outchan;
	  int inchan;
          SV **temp;
          AV *avsub;
	  int len;
	  int i, j;
        CODE:
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
          coeff = mymalloc(sizeof(double) * outchan * inchan);
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
	  RETVAL = i_convert(src, coeff, outchan, inchan);
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

double
i_img_diffd(im1,im2)
    Imager::ImgRaw     im1
    Imager::ImgRaw     im2

int
i_img_samef(im1, im2, epsilon = i_img_epsilonf(), what=NULL)
    Imager::ImgRaw    im1
    Imager::ImgRaw    im2
    double epsilon
    const char *what

double
i_img_epsilonf()

bool
_is_color_object(sv)
	SV* sv
    CODE:
        SvGETMAGIC(sv);
        RETVAL = SvOK(sv) && SvROK(sv) &&
	   (sv_derived_from(sv, "Imager::Color")
          || sv_derived_from(sv, "Imager::Color::Float"));
    OUTPUT:
        RETVAL

#ifdef HAVE_LIBTT


Imager::Font::TT
i_tt_new(fontname)
	      char*     fontname


MODULE = Imager         PACKAGE = Imager::Font::TT      PREFIX=TT_

#define TT_DESTROY(handle) i_tt_destroy(handle)

void
TT_DESTROY(handle)
     Imager::Font::TT   handle

int
TT_CLONE_SKIP(...)
    CODE:
        (void)items; /* avoid unused warning */
        RETVAL = 1;
    OUTPUT:
        RETVAL


MODULE = Imager         PACKAGE = Imager


undef_int
i_tt_text(handle,im,xb,yb,cl,points,str_sv,len_ignored,smooth,utf8,align=1)
  Imager::Font::TT     handle
    Imager::ImgRaw     im
	       i_img_dim     xb
	       i_img_dim     yb
     Imager::Color     cl
             double     points
	      SV *     str_sv
	       int     smooth
               int     utf8
               int     align
             PREINIT:
               char *str;
               STRLEN len;
             CODE:
#ifdef SvUTF8
               if (SvUTF8(str_sv))
                 utf8 = 1;
#endif
               str = SvPV(str_sv, len);
               RETVAL = i_tt_text(handle, im, xb, yb, cl, points, str, 
                                  len, smooth, utf8, align);
             OUTPUT:
               RETVAL                


undef_int
i_tt_cp(handle,im,xb,yb,channel,points,str_sv,len_ignored,smooth,utf8,align=1)
  Imager::Font::TT     handle
    Imager::ImgRaw     im
	       i_img_dim     xb
	       i_img_dim     yb
	       int     channel
             double     points
	      SV *     str_sv
	       int     smooth
               int     utf8
               int     align
             PREINIT:
               char *str;
               STRLEN len;
             CODE:
#ifdef SvUTF8
               if (SvUTF8(str_sv))
                 utf8 = 1;
#endif
               str = SvPV(str_sv, len);
               RETVAL = i_tt_cp(handle, im, xb, yb, channel, points, str, len,
                                smooth, utf8, align);
             OUTPUT:
                RETVAL


void
i_tt_bbox(handle,point,str_sv,len_ignored, utf8)
  Imager::Font::TT     handle
	     double     point
	       SV*    str_sv
               int     utf8
	     PREINIT:
	       i_img_dim cords[BOUNDING_BOX_COUNT];
	       int rc;
               char *  str;
               STRLEN len;
               int i;
	     PPCODE:
#ifdef SvUTF8
               if (SvUTF8(ST(2)))
                 utf8 = 1;
#endif
               str = SvPV(str_sv, len);
  	       if ((rc=i_tt_bbox(handle,point,str,len,cords, utf8))) {
                 EXTEND(SP, rc);
                 for (i = 0; i < rc; ++i) {
                   PUSHs(sv_2mortal(newSViv(cords[i])));
                 }
               }

void
i_tt_has_chars(handle, text_sv, utf8)
        Imager::Font::TT handle
        SV  *text_sv
        int utf8
      PREINIT:
        char const *text;
        STRLEN len;
        char *work;
        size_t count;
        size_t i;
      PPCODE:
#ifdef SvUTF8
        if (SvUTF8(text_sv))
          utf8 = 1;
#endif
        text = SvPV(text_sv, len);
        work = mymalloc(len);
        count = i_tt_has_chars(handle, text, len, utf8, work);
        if (GIMME_V == G_ARRAY) {
          EXTEND(SP, count);
          for (i = 0; i < count; ++i) {
            PUSHs(boolSV(work[i]));
          }
        }
        else {
          EXTEND(SP, 1);
          PUSHs(sv_2mortal(newSVpv(work, count)));
        }
        myfree(work);

void
i_tt_dump_names(handle)
        Imager::Font::TT handle

void
i_tt_face_name(handle)
        Imager::Font::TT handle
      PREINIT:
        char name[255];
        size_t len;
      PPCODE:
        len = i_tt_face_name(handle, name, sizeof(name));
        if (len) {
          EXTEND(SP, 1);
          PUSHs(sv_2mortal(newSVpv(name, len-1)));
        }

void
i_tt_glyph_name(handle, text_sv, utf8 = 0)
        Imager::Font::TT handle
        SV *text_sv
        int utf8
      PREINIT:
        char const *text;
        STRLEN work_len;
        size_t len;
        size_t outsize;
        char name[255];
      PPCODE:
#ifdef SvUTF8
        if (SvUTF8(text_sv))
          utf8 = 1;
#endif
        text = SvPV(text_sv, work_len);
        len = work_len;
        while (len) {
          unsigned long ch;
          if (utf8) {
            ch = i_utf8_advance(&text, &len);
            if (ch == ~0UL) {
              i_push_error(0, "invalid UTF8 character");
              break;
            }
          }
          else {
            ch = *text++;
            --len;
          }
          EXTEND(SP, 1);
          if ((outsize = i_tt_glyph_name(handle, ch, name, sizeof(name))) != 0) {
            PUSHs(sv_2mortal(newSVpv(name, 0)));
          }
          else {
            PUSHs(&PL_sv_undef);
          } 
        }

#endif 

const char *
i_test_format_probe(ig, length)
        Imager::IO     ig
	       int     length

Imager::ImgRaw
i_readpnm_wiol(ig, allow_incomplete)
        Imager::IO     ig
	       int     allow_incomplete


void
i_readpnm_multi_wiol(ig, allow_incomplete)
        Imager::IO ig
	       int     allow_incomplete
      PREINIT:
        i_img **imgs;
        int count=0;
        int i;
      PPCODE:
        imgs = i_readpnm_multi_wiol(ig, &count, allow_incomplete);
        if (imgs) {
          EXTEND(SP, count);
          for (i = 0; i < count; ++i) {
            SV *sv = sv_newmortal();
            sv_setref_pv(sv, "Imager::ImgRaw", (void *)imgs[i]);
            PUSHs(sv);
          }
          myfree(imgs);
        }

undef_int
i_writeppm_wiol(im, ig)
    Imager::ImgRaw     im
        Imager::IO     ig





Imager::ImgRaw
i_readraw_wiol(ig,x,y,datachannels,storechannels,intrl)
        Imager::IO     ig
	       i_img_dim     x
	       i_img_dim     y
	       int     datachannels
	       int     storechannels
	       int     intrl

undef_int
i_writeraw_wiol(im,ig)
    Imager::ImgRaw     im
        Imager::IO     ig

undef_int
i_writebmp_wiol(im,ig)
    Imager::ImgRaw     im
        Imager::IO     ig

Imager::ImgRaw
i_readbmp_wiol(ig, allow_incomplete=0)
        Imager::IO     ig
        int            allow_incomplete


undef_int
i_writetga_wiol(im,ig, wierdpack, compress, idstring)
    Imager::ImgRaw     im
        Imager::IO     ig
               int     wierdpack
               int     compress
              char*    idstring
            PREINIT:
                int idlen;
	       CODE:
                idlen  = SvCUR(ST(4));
                RETVAL = i_writetga_wiol(im, ig, wierdpack, compress, idstring, idlen);
                OUTPUT:
                RETVAL


Imager::ImgRaw
i_readtga_wiol(ig, length)
        Imager::IO     ig
               int     length




Imager::ImgRaw
i_scaleaxis(im,Value,Axis)
    Imager::ImgRaw     im
             double     Value
	       int     Axis

Imager::ImgRaw
i_scale_nn(im,scx,scy)
    Imager::ImgRaw     im
             double    scx
             double    scy

Imager::ImgRaw
i_scale_mixing(im, width, height)
    Imager::ImgRaw     im
	       i_img_dim     width
	       i_img_dim     height

Imager::ImgRaw
i_haar(im)
    Imager::ImgRaw     im

int
i_count_colors(im,maxc)
    Imager::ImgRaw     im
               int     maxc

void
i_get_anonymous_color_histo(im, maxc = 0x40000000)
   Imager::ImgRaw  im
   int maxc
    PREINIT:
        int i;
        unsigned int * col_usage = NULL;
        int col_cnt;
    PPCODE:
	col_cnt = i_get_anonymous_color_histo(im, &col_usage, maxc);
        EXTEND(SP, col_cnt);
        for (i = 0; i < col_cnt; i++)  {
            PUSHs(sv_2mortal(newSViv( col_usage[i])));
        }
        myfree(col_usage);
        XSRETURN(col_cnt);


void
i_transform(im,opx,opy,parm)
    Imager::ImgRaw     im
             PREINIT:
             double* parm;
             int *opx;
             int *opy;
             int     opxl;
             int     opyl;
             int     parmlen;
             AV* av;
             SV* sv1;
             int i;
	     i_img *result;
             PPCODE:
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
             result=i_transform(im,opx,opxl,opy,opyl,parm,parmlen);
             myfree(parm);
             myfree(opy);
             myfree(opx);
 	     if (result) {
	       SV *result_sv = sv_newmortal();
	       EXTEND(SP, 1);
	       sv_setref_pv(result_sv, "Imager::ImgRaw", (void*)result);
	       PUSHs(result_sv);
	     }

void
i_transform2(sv_width,sv_height,channels,sv_ops,av_n_regs,av_c_regs,av_in_imgs)
	SV *sv_width
	SV *sv_height
	SV *sv_ops
	AV *av_n_regs
	AV *av_c_regs
	AV *av_in_imgs
	int channels
	     PREINIT:
             i_img_dim width;
             i_img_dim height;
	     struct rm_op *ops;
	     STRLEN ops_len;
	     int ops_count;
             double *n_regs;
             int n_regs_count;
             i_color *c_regs;
	     int c_regs_count;
             int in_imgs_count;
             i_img **in_imgs;
             SV *sv1;
             IV tmp;
	     int i;
	     i_img *result;
             PPCODE:

             in_imgs_count = av_len(av_in_imgs)+1;
	     for (i = 0; i < in_imgs_count; ++i) {
	       sv1 = *av_fetch(av_in_imgs, i, 0);
	       if (!sv_derived_from(sv1, "Imager::ImgRaw")) {
		 croak("sv_in_img must contain only images");
	       }
	     }
             if (in_imgs_count > 0) {
               in_imgs = mymalloc(in_imgs_count*sizeof(i_img*));
               for (i = 0; i < in_imgs_count; ++i) {              
	         sv1 = *av_fetch(av_in_imgs,i,0);
	         if (!sv_derived_from(sv1, "Imager::ImgRaw")) {
		   croak("Parameter 5 must contain only images");
	         }
                 tmp = SvIV((SV*)SvRV(sv1));
	         in_imgs[i] = INT2PTR(i_img*, tmp);
	       }
	     }
             else {
	       /* no input images */
	       in_imgs = NULL;
             }
             /* default the output size from the first input if possible */
             if (SvOK(sv_width))
	       width = SvIV(sv_width);
             else if (in_imgs_count)
	       width = in_imgs[0]->xsize;
             else
	       croak("No output image width supplied");

             if (SvOK(sv_height))
	       height = SvIV(sv_height);
             else if (in_imgs_count)
	       height = in_imgs[0]->ysize;
             else
	       croak("No output image height supplied");

	     ops = (struct rm_op *)SvPV(sv_ops, ops_len);
             if (ops_len % sizeof(struct rm_op))
	         croak("Imager: Parameter 3 must be a bitmap of regops\n");
	     ops_count = ops_len / sizeof(struct rm_op);

	     n_regs_count = av_len(av_n_regs)+1;
             n_regs = mymalloc(n_regs_count * sizeof(double));
	     for (i = 0; i < n_regs_count; ++i) {
	       sv1 = *av_fetch(av_n_regs,i,0);
	       if (SvOK(sv1))
	         n_regs[i] = SvNV(sv1);
	     }
             c_regs_count = av_len(av_c_regs)+1;
             c_regs = mymalloc(c_regs_count * sizeof(i_color));
             /* I don't bother initializing the colou?r registers */

	     result=i_transform2(width, height, channels, ops, ops_count, 
				 n_regs, n_regs_count, 
				 c_regs, c_regs_count, in_imgs, in_imgs_count);
	     if (in_imgs)
	         myfree(in_imgs);
             myfree(n_regs);
	     myfree(c_regs);
 	     if (result) {
	       SV *result_sv = sv_newmortal();
	       EXTEND(SP, 1);
	       sv_setref_pv(result_sv, "Imager::ImgRaw", (void*)result);
	       PUSHs(result_sv);
	     }


void
i_contrast(im,intensity)
    Imager::ImgRaw     im
             float     intensity

void
i_hardinvert(im)
    Imager::ImgRaw     im

void
i_hardinvertall(im)
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
         i_img_dim     light_x
         i_img_dim     light_y
         i_img_dim     strength


void
i_bumpmap_complex(im,bump,channel,tx,ty,Lx,Ly,Lz,cd,cs,n,Ia,Il,Is)
    Imager::ImgRaw     im
    Imager::ImgRaw     bump
               int     channel
               i_img_dim     tx
               i_img_dim     ty
             double     Lx
             double     Ly
             double     Lz
             float     cd
             float     cs
             float     n
     Imager::Color     Ia
     Imager::Color     Il
     Imager::Color     Is



void
i_postlevels(im,levels)
    Imager::ImgRaw     im
             int       levels

void
i_mosaic(im,size)
    Imager::ImgRaw     im
         i_img_dim     size

void
i_watermark(im,wmark,tx,ty,pixdiff)
    Imager::ImgRaw     im
    Imager::ImgRaw     wmark
               i_img_dim     tx
               i_img_dim     ty
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
	i_img_dim *xo;
	i_img_dim *yo;
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
	xo = mymalloc( sizeof(i_img_dim) * num );
	yo = mymalloc( sizeof(i_img_dim) * num );
	ival = mymalloc( sizeof(i_color) * num );
	for(i = 0; i<num; i++) {
	  xo[i]   = (i_img_dim)SvIV(* av_fetch(axx, i, 0));
	  yo[i]   = (i_img_dim)SvIV(* av_fetch(ayy, i, 0));
          sv = *av_fetch(ac, i, 0);
	  if ( !sv_derived_from(sv, "Imager::Color") ) {
	    free(axx); free(ayy); free(ac);
            croak("i_gradgen: Element of fourth argument is not derived from Imager::Color");
	  }
	  ival[i] = *INT2PTR(i_color *, SvIV((SV *)SvRV(sv)));
	}
        i_gradgen(im, num, xo, yo, ival, dmeasure);
        myfree(xo);
        myfree(yo);
        myfree(ival);

Imager::ImgRaw
i_diff_image(im, im2, mindist=0)
    Imager::ImgRaw     im
    Imager::ImgRaw     im2
            double     mindist

undef_int
i_fountain(im, xa, ya, xb, yb, type, repeat, combine, super_sample, ssample_param, segs)
    Imager::ImgRaw     im
            double     xa
            double     ya
            double     xb
            double     yb
               int     type
               int     repeat
               int     combine
               int     super_sample
            double     ssample_param
      PREINIT:
        AV *asegs;
        int count;
        i_fountain_seg *segs;
      CODE:
	if (!SvROK(ST(10)) || ! SvTYPE(SvRV(ST(10))))
	    croak("i_fountain: argument 11 must be an array ref");
        
	asegs = (AV *)SvRV(ST(10));
        segs = load_fount_segs(aTHX_ asegs, &count);
        RETVAL = i_fountain(im, xa, ya, xb, yb, type, repeat, combine, 
                            super_sample, ssample_param, count, segs);
        myfree(segs);
      OUTPUT:
        RETVAL

Imager::FillHandle
i_new_fill_fount(xa, ya, xb, yb, type, repeat, combine, super_sample, ssample_param, segs)
            double     xa
            double     ya
            double     xb
            double     yb
               int     type
               int     repeat
               int     combine
               int     super_sample
            double     ssample_param
      PREINIT:
        AV *asegs;
        int count;
        i_fountain_seg *segs;
      CODE:
	if (!SvROK(ST(9)) || ! SvTYPE(SvRV(ST(9))))
	    croak("i_fountain: argument 11 must be an array ref");
        
	asegs = (AV *)SvRV(ST(9));
        segs = load_fount_segs(aTHX_ asegs, &count);
        RETVAL = i_new_fill_fount(xa, ya, xb, yb, type, repeat, combine, 
                                  super_sample, ssample_param, count, segs);
        myfree(segs);        
      OUTPUT:
        RETVAL

Imager::FillHandle
i_new_fill_opacity(other_fill, alpha_mult)
    Imager::FillHandle other_fill
    double alpha_mult

void
i_errors()
      PREINIT:
        i_errmsg *errors;
	int i;
	AV *av;
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
i_clear_error()

void
i_push_error(code, msg)
	int code
	const char *msg

undef_int
i_nearest_color(im, ...)
    Imager::ImgRaw     im
      PREINIT:
	int num;
	i_img_dim *xo;
	i_img_dim *yo;
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
	xo = mymalloc( sizeof(i_img_dim) * num );
	yo = mymalloc( sizeof(i_img_dim) * num );
	ival = mymalloc( sizeof(i_color) * num );
	for(i = 0; i<num; i++) {
	  xo[i]   = (i_img_dim)SvIV(* av_fetch(axx, i, 0));
	  yo[i]   = (i_img_dim)SvIV(* av_fetch(ayy, i, 0));
          sv = *av_fetch(ac, i, 0);
	  if ( !sv_derived_from(sv, "Imager::Color") ) {
	    free(axx); free(ayy); free(ac);
            croak("i_nearest_color: Element of fourth argument is not derived from Imager::Color");
	  }
	  ival[i] = *INT2PTR(i_color *, SvIV((SV *)SvRV(sv)));
	}
        RETVAL = i_nearest_color(im, num, xo, yo, ival, dmeasure);
      OUTPUT:
        RETVAL

void
malloc_state()

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
                   PUSHs(sv_2mortal(newSViv(PTR2IV(rc))));
                   PUSHs(sv_2mortal(newSVpvn(evstr, strlen(evstr))));
                 } else {
                   EXTEND(SP,1);
                   PUSHs(sv_2mortal(newSViv(PTR2IV(rc))));
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
	       func_ptr *functions;
	     PPCODE:
	       dso_handle=(DSO_handle*)dso_handle_v;
	       functions = DSO_funclist(dso_handle);
	       i=0;
	       while( functions[i].name != NULL) {
	         EXTEND(SP,1);
		 PUSHs(sv_2mortal(newSVpv(functions[i].name,0)));
	         EXTEND(SP,1);
		 PUSHs(sv_2mortal(newSVpv(functions[i++].pcode,0)));
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

SV *
i_get_pixel(im, x, y)
	Imager::ImgRaw im
	i_img_dim x
	i_img_dim y;
      PREINIT:
        i_color *color;
      CODE:
	color = (i_color *)mymalloc(sizeof(i_color));
	if (i_gpix(im, x, y, color) == 0) {
          RETVAL = NEWSV(0, 0);
          sv_setref_pv(RETVAL, "Imager::Color", (void *)color);
        }
        else {
          myfree(color);
          RETVAL = &PL_sv_undef;
        }
      OUTPUT:
        RETVAL
        

int
i_ppix(im, x, y, cl)
        Imager::ImgRaw im
        i_img_dim x
        i_img_dim y
        Imager::Color cl

Imager::ImgRaw
i_img_pal_new(x, y, channels, maxpal)
	i_img_dim x
        i_img_dim y
        int     channels
	int	maxpal

Imager::ImgRaw
i_img_to_pal(src, quant)
        Imager::ImgRaw src
      PREINIT:
        HV *hv;
        i_quantize quant;
      CODE:
        if (!SvROK(ST(1)) || ! SvTYPE(SvRV(ST(1))))
          croak("i_img_to_pal: second argument must be a hash ref");
        hv = (HV *)SvRV(ST(1));
        memset(&quant, 0, sizeof(quant));
	quant.version = 1;
        quant.mc_size = 256;
	ip_handle_quant_opts(aTHX_ &quant, hv);
        RETVAL = i_img_to_pal(src, &quant);
        if (RETVAL) {
          ip_copy_colors_back(aTHX_ hv, &quant);
        }
	ip_cleanup_quant_opts(aTHX_ &quant);
      OUTPUT:
        RETVAL

Imager::ImgRaw
i_img_to_rgb(src)
        Imager::ImgRaw src

void
i_gpal(im, l, r, y)
        Imager::ImgRaw  im
        i_img_dim     l
        i_img_dim     r
        i_img_dim     y
      PREINIT:
        i_palidx *work;
        int count, i;
      PPCODE:
        if (l < r) {
          work = mymalloc((r-l) * sizeof(i_palidx));
          count = i_gpal(im, l, r, y, work);
          if (GIMME_V == G_ARRAY) {
            EXTEND(SP, count);
            for (i = 0; i < count; ++i) {
              PUSHs(sv_2mortal(newSViv(work[i])));
            }
          }
          else {
            EXTEND(SP, 1);
            PUSHs(sv_2mortal(newSVpv((char *)work, count * sizeof(i_palidx))));
          }
          myfree(work);
        }
        else {
          if (GIMME_V != G_ARRAY) {
            EXTEND(SP, 1);
            PUSHs(&PL_sv_undef);
          }
        }

int
i_ppal(im, l, y, ...)
        Imager::ImgRaw  im
        i_img_dim     l
        i_img_dim     y
      PREINIT:
        i_palidx *work;
        i_img_dim i;
      CODE:
        if (items > 3) {
          work = malloc_temp(aTHX_ sizeof(i_palidx) * (items-3));
          for (i=0; i < items-3; ++i) {
            work[i] = SvIV(ST(i+3));
          }
          validate_i_ppal(im, work, items - 3);
          RETVAL = i_ppal(im, l, l+items-3, y, work);
        }
        else {
          RETVAL = 0;
        }
      OUTPUT:
        RETVAL

int
i_ppal_p(im, l, y, data)
        Imager::ImgRaw  im
        i_img_dim     l
        i_img_dim     y
        SV *data
      PREINIT:
        i_palidx const *work;
        STRLEN len;
      CODE:
        work = (i_palidx const *)SvPV(data, len);
        len /= sizeof(i_palidx);
        if (len > 0) {
          validate_i_ppal(im, work, len);
          RETVAL = i_ppal(im, l, l+len, y, work);
        }
        else {
          RETVAL = 0;
        }
      OUTPUT:
        RETVAL

SV *
i_addcolors(im, ...)
        Imager::ImgRaw  im
      PREINIT:
        int index;
        i_color *colors;
        int i;
      CODE:
        if (items < 2)
          croak("i_addcolors: no colors to add");
        colors = mymalloc((items-1) * sizeof(i_color));
        for (i=0; i < items-1; ++i) {
          if (sv_isobject(ST(i+1)) 
              && sv_derived_from(ST(i+1), "Imager::Color")) {
            IV tmp = SvIV((SV *)SvRV(ST(i+1)));
            colors[i] = *INT2PTR(i_color *, tmp);
          }
          else {
            myfree(colors);
            croak("i_addcolor: pixels must be Imager::Color objects");
          }
        }
        index = i_addcolors(im, colors, items-1);
        myfree(colors);
        if (index == 0) {
          RETVAL = newSVpv("0 but true", 0);
        }
        else if (index == -1) {
          RETVAL = &PL_sv_undef;
        }
        else {
          RETVAL = newSViv(index);
        }
      OUTPUT:
        RETVAL

undef_int 
i_setcolors(im, index, ...)
        Imager::ImgRaw  im
        int index
      PREINIT:
        i_color *colors;
        int i;
      CODE:
        if (items < 3)
          croak("i_setcolors: no colors to add");
        colors = mymalloc((items-2) * sizeof(i_color));
        for (i=0; i < items-2; ++i) {
          if (sv_isobject(ST(i+2)) 
              && sv_derived_from(ST(i+2), "Imager::Color")) {
            IV tmp = SvIV((SV *)SvRV(ST(i+2)));
            colors[i] = *INT2PTR(i_color *, tmp);
          }
          else {
            myfree(colors);
            croak("i_setcolors: pixels must be Imager::Color objects");
          }
        }
        RETVAL = i_setcolors(im, index, colors, items-2);
        myfree(colors);
      OUTPUT:
	RETVAL

void
i_getcolors(im, index, ...)
        Imager::ImgRaw im
        int index
      PREINIT:
        i_color *colors;
        int count = 1;
        int i;
      PPCODE:
        if (items > 3)
          croak("i_getcolors: too many arguments");
        if (items == 3)
          count = SvIV(ST(2));
        if (count < 1)
          croak("i_getcolors: count must be positive");
        colors = mymalloc(sizeof(i_color) * count);
        if (i_getcolors(im, index, colors, count)) {
          for (i = 0; i < count; ++i) {
            i_color *pv;
            SV *sv = sv_newmortal();
            pv = mymalloc(sizeof(i_color));
            *pv = colors[i];
            sv_setref_pv(sv, "Imager::Color", (void *)pv);
            PUSHs(sv);
          }
        }
        myfree(colors);


undef_neg_int
i_colorcount(im)
        Imager::ImgRaw im

undef_neg_int
i_maxcolors(im)
        Imager::ImgRaw im

SV *
i_findcolor(im, color)
        Imager::ImgRaw im
        Imager::Color color
      PREINIT:
        i_palidx index;
      CODE:
        if (i_findcolor(im, color, &index)) {
          RETVAL = newSViv(index);
        }
        else {
          RETVAL = &PL_sv_undef;
        }
      OUTPUT:
        RETVAL

int
i_img_bits(im)
        Imager::ImgRaw  im

int
i_img_type(im)
        Imager::ImgRaw  im

int
i_img_virtual(im)
        Imager::ImgRaw  im

void
i_gsamp(im, l, r, y, ...)
        Imager::ImgRaw im
        i_img_dim l
        i_img_dim r
        i_img_dim y
      PREINIT:
        int *chans;
        int chan_count;
        i_sample_t *data;
        i_img_dim count, i;
      PPCODE:
        if (items < 5)
          croak("No channel numbers supplied to g_samp()");
        if (l < r) {
          chan_count = items - 4;
          chans = mymalloc(sizeof(int) * chan_count);
          for (i = 0; i < chan_count; ++i)
            chans[i] = SvIV(ST(i+4));
          data = mymalloc(sizeof(i_sample_t) * (r-l) * chan_count); /* XXX: memleak? */
          count = i_gsamp(im, l, r, y, data, chans, chan_count);
	  myfree(chans);
          if (GIMME_V == G_ARRAY) {
            EXTEND(SP, count);
            for (i = 0; i < count; ++i)
              PUSHs(sv_2mortal(newSViv(data[i])));
          }
          else {
            EXTEND(SP, 1);
            PUSHs(sv_2mortal(newSVpv((char *)data, count * sizeof(i_sample_t))));
          }
	  myfree(data);
        }
        else {
          if (GIMME_V != G_ARRAY) {
            EXTEND(SP, 1);
            PUSHs(&PL_sv_undef);
          }
        }

undef_neg_int
i_gsamp_bits(im, l, r, y, bits, target, offset, ...)
        Imager::ImgRaw im
        i_img_dim l
        i_img_dim r
        i_img_dim y
	int bits
	AV *target
	STRLEN offset
      PREINIT:
        int *chans;
        int chan_count;
        unsigned *data;
        i_img_dim count, i;
      CODE:
	i_clear_error();
        if (items < 8)
          croak("No channel numbers supplied to g_samp()");
        if (l < r) {
          chan_count = items - 7;
          chans = mymalloc(sizeof(int) * chan_count);
          for (i = 0; i < chan_count; ++i)
            chans[i] = SvIV(ST(i+7));
          data = mymalloc(sizeof(unsigned) * (r-l) * chan_count);
          count = i_gsamp_bits(im, l, r, y, data, chans, chan_count, bits);
	  myfree(chans);
	  for (i = 0; i < count; ++i) {
	    av_store(target, i+offset, newSVuv(data[i]));
	  }
	  myfree(data);
	  RETVAL = count;
        }
        else {
	  RETVAL = 0;
        }
      OUTPUT:
	RETVAL

undef_neg_int
i_psamp_bits(im, l, y, bits, channels_sv, data_av, data_offset = 0, pixel_count = -1)
        Imager::ImgRaw im
        i_img_dim l
        i_img_dim y
	int bits
	SV *channels_sv
	AV *data_av
        int data_offset
        int pixel_count
      PREINIT:
	int chan_count;
	int *channels;
	STRLEN data_count;
	size_t data_used;
	unsigned *data;
	ptrdiff_t i;
      CODE:
	i_clear_error();
	if (SvOK(channels_sv)) {
	  AV *channels_av;
	  if (!SvROK(channels_sv) || SvTYPE(SvRV(channels_sv)) != SVt_PVAV) {
	    croak("channels is not an array ref");
	  }
	  channels_av = (AV *)SvRV(channels_sv);
 	  chan_count = av_len(channels_av) + 1;
	  if (chan_count < 1) {
	    croak("i_psamp_bits: no channels provided");
	  }
	  channels = mymalloc(sizeof(int) * chan_count);
 	  for (i = 0; i < chan_count; ++i)
	    channels[i] = SvIV(*av_fetch(channels_av, i, 0));
        }
	else {
	  chan_count = im->channels;
	  channels = NULL;
	}

	data_count = av_len(data_av) + 1;
	if (data_offset < 0) {
	  croak("data_offset must by non-negative");
	}
	if (data_offset > data_count) {
	  croak("data_offset greater than number of samples supplied");
        }
	if (pixel_count == -1 || 
	    data_offset + pixel_count * chan_count > data_count) {
	  pixel_count = (data_count - data_offset) / chan_count;
	}

	data_used = pixel_count * chan_count;
	data = mymalloc(sizeof(unsigned) * data_count);
	for (i = 0; i < data_used; ++i)
	  data[i] = SvUV(*av_fetch(data_av, data_offset + i, 0));

	RETVAL = i_psamp_bits(im, l, l + pixel_count, y, data, channels, 
	                      chan_count, bits);

	if (data)
	  myfree(data);
	if (channels)
	  myfree(channels);
      OUTPUT:
	RETVAL

Imager::ImgRaw
i_img_masked_new(targ, mask, x, y, w, h)
        Imager::ImgRaw targ
        i_img_dim x
        i_img_dim y
        i_img_dim w
        i_img_dim h
      PREINIT:
        i_img *mask;
      CODE:
        if (SvOK(ST(1))) {
          if (!sv_isobject(ST(1)) 
              || !sv_derived_from(ST(1), "Imager::ImgRaw")) {
            croak("i_img_masked_new: parameter 2 must undef or an image");
          }
          mask = INT2PTR(i_img *, SvIV((SV *)SvRV(ST(1))));
        }
        else
          mask = NULL;
        RETVAL = i_img_masked_new(targ, mask, x, y, w, h);
      OUTPUT:
        RETVAL

int
i_plin(im, l, y, ...)
        Imager::ImgRaw  im
        i_img_dim     l
        i_img_dim     y
      PREINIT:
        i_color *work;
        STRLEN i;
        STRLEN len;
        size_t count;
      CODE:
        if (items > 3) {
          if (items == 4 && SvOK(ST(3)) && !SvROK(ST(3))) {
	    /* supplied as a byte string */
            work = (i_color *)SvPV(ST(3), len);
            count = len / sizeof(i_color);
	    if (count * sizeof(i_color) != len) {
              croak("i_plin: length of scalar argument must be multiple of sizeof i_color");
            }
            RETVAL = i_plin(im, l, l+count, y, work);
          }
	  else {
            work = mymalloc(sizeof(i_color) * (items-3));
            for (i=0; i < items-3; ++i) {
              if (sv_isobject(ST(i+3)) 
                  && sv_derived_from(ST(i+3), "Imager::Color")) {
                IV tmp = SvIV((SV *)SvRV(ST(i+3)));
                work[i] = *INT2PTR(i_color *, tmp);
              }
              else {
                myfree(work);
                croak("i_plin: pixels must be Imager::Color objects");
              }
            }
            RETVAL = i_plin(im, l, l+items-3, y, work);
            myfree(work);
          }
        }
        else {
          RETVAL = 0;
        }
      OUTPUT:
        RETVAL

int
i_ppixf(im, x, y, cl)
        Imager::ImgRaw im
        i_img_dim x
        i_img_dim y
        Imager::Color::Float cl

void
i_gsampf(im, l, r, y, ...)
        Imager::ImgRaw im
        i_img_dim l
        i_img_dim r
        i_img_dim y
      PREINIT:
        int *chans;
        int chan_count;
        i_fsample_t *data;
        i_img_dim count, i;
      PPCODE:
        if (items < 5)
          croak("No channel numbers supplied to g_sampf()");
        if (l < r) {
          chan_count = items - 4;
          chans = mymalloc(sizeof(int) * chan_count);
          for (i = 0; i < chan_count; ++i)
            chans[i] = SvIV(ST(i+4));
          data = mymalloc(sizeof(i_fsample_t) * (r-l) * chan_count);
          count = i_gsampf(im, l, r, y, data, chans, chan_count);
          myfree(chans);
          if (GIMME_V == G_ARRAY) {
            EXTEND(SP, count);
            for (i = 0; i < count; ++i)
              PUSHs(sv_2mortal(newSVnv(data[i])));
          }
          else {
            EXTEND(SP, 1);
            PUSHs(sv_2mortal(newSVpv((void *)data, count * sizeof(i_fsample_t))));
          }
          myfree(data);
        }
        else {
          if (GIMME_V != G_ARRAY) {
            EXTEND(SP, 1);
            PUSHs(&PL_sv_undef);
          }
        }

int
i_plinf(im, l, y, ...)
        Imager::ImgRaw  im
        i_img_dim     l
        i_img_dim     y
      PREINIT:
        i_fcolor *work;
        i_img_dim i;
        STRLEN len;
        size_t count;
      CODE:
        if (items > 3) {
          if (items == 4 && SvOK(ST(3)) && !SvROK(ST(3))) {
	    /* supplied as a byte string */
            work = (i_fcolor *)SvPV(ST(3), len);
            count = len / sizeof(i_fcolor);
	    if (count * sizeof(i_fcolor) != len) {
              croak("i_plin: length of scalar argument must be multiple of sizeof i_fcolor");
            }
            RETVAL = i_plinf(im, l, l+count, y, work);
          }
	  else {
            work = mymalloc(sizeof(i_fcolor) * (items-3));
            for (i=0; i < items-3; ++i) {
              if (sv_isobject(ST(i+3)) 
                  && sv_derived_from(ST(i+3), "Imager::Color::Float")) {
                IV tmp = SvIV((SV *)SvRV(ST(i+3)));
                work[i] = *INT2PTR(i_fcolor *, tmp);
              }
              else {
                myfree(work);
                croak("i_plinf: pixels must be Imager::Color::Float objects");
              }
            }
            /**(char *)0 = 1;*/
            RETVAL = i_plinf(im, l, l+items-3, y, work);
            myfree(work);
          }
        }
        else {
          RETVAL = 0;
        }
      OUTPUT:
        RETVAL

SV *
i_gpixf(im, x, y)
	Imager::ImgRaw im
	i_img_dim x
	i_img_dim y;
      PREINIT:
        i_fcolor *color;
      CODE:
	color = (i_fcolor *)mymalloc(sizeof(i_fcolor));
	if (i_gpixf(im, x, y, color) == 0) {
          RETVAL = NEWSV(0,0);
          sv_setref_pv(RETVAL, "Imager::Color::Float", (void *)color);
        }
        else {
          myfree(color);
          RETVAL = &PL_sv_undef;
        }
      OUTPUT:
        RETVAL

void
i_glin(im, l, r, y)
        Imager::ImgRaw im
        i_img_dim l
        i_img_dim r
        i_img_dim y
      PREINIT:
        i_color *vals;
        i_img_dim count, i;
      PPCODE:
        if (l < r) {
          vals = mymalloc((r-l) * sizeof(i_color));
          memset(vals, 0, (r-l) * sizeof(i_color));
          count = i_glin(im, l, r, y, vals);
	  if (GIMME_V == G_ARRAY) {
            EXTEND(SP, count);
            for (i = 0; i < count; ++i) {
              SV *sv;
              i_color *col = mymalloc(sizeof(i_color));
              *col = vals[i];
              sv = sv_newmortal();
              sv_setref_pv(sv, "Imager::Color", (void *)col);
              PUSHs(sv);
            }
          }
          else if (count) {
	    EXTEND(SP, 1);
	    PUSHs(sv_2mortal(newSVpv((void *)vals, count * sizeof(i_color))));
          }
          myfree(vals);
        }

void
i_glinf(im, l, r, y)
        Imager::ImgRaw im
        i_img_dim l
        i_img_dim r
        i_img_dim y
      PREINIT:
        i_fcolor *vals;
        i_img_dim count, i;
        i_fcolor zero;
      PPCODE:
	for (i = 0; i < MAXCHANNELS; ++i)
	  zero.channel[i] = 0;
        if (l < r) {
          vals = mymalloc((r-l) * sizeof(i_fcolor));
          for (i = 0; i < r-l; ++i)
	    vals[i] = zero;
          count = i_glinf(im, l, r, y, vals);
          if (GIMME_V == G_ARRAY) {
            EXTEND(SP, count);
            for (i = 0; i < count; ++i) {
              SV *sv;
              i_fcolor *col = mymalloc(sizeof(i_fcolor));
              *col = vals[i];
              sv = sv_newmortal();
              sv_setref_pv(sv, "Imager::Color::Float", (void *)col);
              PUSHs(sv);
            }
          }
          else if (count) {
            EXTEND(SP, 1);
            PUSHs(sv_2mortal(newSVpv((void *)vals, count * sizeof(i_fcolor))));
          }
          myfree(vals);
        }

Imager::ImgRaw
i_img_16_new(x, y, ch)
        i_img_dim x
        i_img_dim y
        int ch

Imager::ImgRaw
i_img_to_rgb16(im)
       Imager::ImgRaw im

Imager::ImgRaw
i_img_double_new(x, y, ch)
        i_img_dim x
        i_img_dim y
        int ch

Imager::ImgRaw
i_img_to_drgb(im)
       Imager::ImgRaw im

undef_int
i_tags_addn(im, name, code, idata)
        Imager::ImgRaw im
        int     code
        int     idata
      PREINIT:
        char *name;
        STRLEN len;
      CODE:
        if (SvOK(ST(1)))
          name = SvPV(ST(1), len);
        else
          name = NULL;
        RETVAL = i_tags_addn(&im->tags, name, code, idata);
      OUTPUT:
        RETVAL

undef_int
i_tags_add(im, name, code, data, idata)
        Imager::ImgRaw  im
        int code
        int idata
      PREINIT:
        char *name;
        char *data;
        STRLEN len;
      CODE:
        if (SvOK(ST(1)))
          name = SvPV(ST(1), len);
        else
          name = NULL;
        if (SvOK(ST(3)))
          data = SvPV(ST(3), len);
        else {
          data = NULL;
          len = 0;
        }
        RETVAL = i_tags_add(&im->tags, name, code, data, len, idata);
      OUTPUT:
        RETVAL

SV *
i_tags_find(im, name, start)
        Imager::ImgRaw  im
        char *name
        int start
      PREINIT:
        int entry;
      CODE:
        if (i_tags_find(&im->tags, name, start, &entry)) {
          if (entry == 0)
            RETVAL = newSVpv("0 but true", 0);
          else
            RETVAL = newSViv(entry);
        } else {
          RETVAL = &PL_sv_undef;
        }
      OUTPUT:
        RETVAL

SV *
i_tags_findn(im, code, start)
        Imager::ImgRaw  im
        int             code
        int             start
      PREINIT:
        int entry;
      CODE:
        if (i_tags_findn(&im->tags, code, start, &entry)) {
          if (entry == 0)
            RETVAL = newSVpv("0 but true", 0);
          else
            RETVAL = newSViv(entry);
        }
        else {
          RETVAL = &PL_sv_undef;
        }
      OUTPUT:
        RETVAL

int
i_tags_delete(im, entry)
        Imager::ImgRaw  im
        int             entry
      CODE:
        RETVAL = i_tags_delete(&im->tags, entry);
      OUTPUT:
        RETVAL

int
i_tags_delbyname(im, name)
        Imager::ImgRaw  im
        char *          name
      CODE:
        RETVAL = i_tags_delbyname(&im->tags, name);
      OUTPUT:
        RETVAL

int
i_tags_delbycode(im, code)
        Imager::ImgRaw  im
        int             code
      CODE:
        RETVAL = i_tags_delbycode(&im->tags, code);
      OUTPUT:
        RETVAL

void
i_tags_get(im, index)
        Imager::ImgRaw  im
        int             index
      PPCODE:
        if (index >= 0 && index < im->tags.count) {
          i_img_tag *entry = im->tags.tags + index;
          EXTEND(SP, 5);
        
          if (entry->name) {
            PUSHs(sv_2mortal(newSVpv(entry->name, 0)));
          }
          else {
            PUSHs(sv_2mortal(newSViv(entry->code)));
          }
          if (entry->data) {
            PUSHs(sv_2mortal(newSVpvn(entry->data, entry->size)));
          }
          else {
            PUSHs(sv_2mortal(newSViv(entry->idata)));
          }
        }

void
i_tags_get_string(im, what_sv)
        Imager::ImgRaw  im
        SV *what_sv
      PREINIT:
        char const *name = NULL;
        int code;
        char buffer[200];
      PPCODE:
        if (SvIOK(what_sv)) {
          code = SvIV(what_sv);
          name = NULL;
        }
        else {
          name = SvPV_nolen(what_sv);
          code = 0;
        }
        if (i_tags_get_string(&im->tags, name, code, buffer, sizeof(buffer))) {
          EXTEND(SP, 1);
          PUSHs(sv_2mortal(newSVpv(buffer, 0)));
        }

int
i_tags_count(im)
        Imager::ImgRaw  im
      CODE:
        RETVAL = im->tags.count;
      OUTPUT:
        RETVAL



MODULE = Imager         PACKAGE = Imager::FillHandle PREFIX=IFILL_

void
IFILL_DESTROY(fill)
        Imager::FillHandle fill

int
IFILL_CLONE_SKIP(...)
    CODE:
        (void)items; /* avoid unused warning for XS variable */
        RETVAL = 1;
    OUTPUT:
        RETVAL

MODULE = Imager         PACKAGE = Imager

Imager::FillHandle
i_new_fill_solid(cl, combine)
        Imager::Color cl
        int combine

Imager::FillHandle
i_new_fill_solidf(cl, combine)
        Imager::Color::Float cl
        int combine

Imager::FillHandle
i_new_fill_hatch(fg, bg, combine, hatch, cust_hatch, dx, dy)
        Imager::Color fg
        Imager::Color bg
        int combine
        int hatch
        i_img_dim dx
        i_img_dim dy
      PREINIT:
        unsigned char *cust_hatch;
        STRLEN len;
      CODE:
        if (SvOK(ST(4))) {
          cust_hatch = (unsigned char *)SvPV(ST(4), len);
        }
        else
          cust_hatch = NULL;
        RETVAL = i_new_fill_hatch(fg, bg, combine, hatch, cust_hatch, dx, dy);
      OUTPUT:
        RETVAL

Imager::FillHandle
i_new_fill_hatchf(fg, bg, combine, hatch, cust_hatch, dx, dy)
        Imager::Color::Float fg
        Imager::Color::Float bg
        int combine
        int hatch
        i_img_dim dx
        i_img_dim dy
      PREINIT:
        unsigned char *cust_hatch;
        STRLEN len;
      CODE:
        if (SvOK(ST(4))) {
          cust_hatch = (unsigned char *)SvPV(ST(4), len);
        }
        else
          cust_hatch = NULL;
        RETVAL = i_new_fill_hatchf(fg, bg, combine, hatch, cust_hatch, dx, dy);
      OUTPUT:
        RETVAL

Imager::FillHandle
i_new_fill_image(src, matrix, xoff, yoff, combine)
        Imager::ImgRaw src
        i_img_dim xoff
        i_img_dim yoff
        int combine
      PREINIT:
        double matrix[9];
        double *matrixp;
        AV *av;
        IV len;
        SV *sv1;
        int i;
      CODE:
        if (!SvOK(ST(1))) {
          matrixp = NULL;
        }
        else {
          if (!SvROK(ST(1)) || SvTYPE(SvRV(ST(1))) != SVt_PVAV)
            croak("i_new_fill_image: parameter must be an arrayref");
	  av=(AV*)SvRV(ST(1));
	  len=av_len(av)+1;
          if (len > 9)
            len = 9;
          for (i = 0; i < len; ++i) {
	    sv1=(*(av_fetch(av,i,0)));
	    matrix[i] = SvNV(sv1);
          }
          for (; i < 9; ++i)
            matrix[i] = 0;
          matrixp = matrix;
        }
        RETVAL = i_new_fill_image(src, matrixp, xoff, yoff, combine);
      OUTPUT:
        RETVAL

MODULE = Imager  PACKAGE = Imager::Internal::Hlines  PREFIX=i_int_hlines_

# this class is only exposed for testing

int
i_int_hlines_testing()

#if i_int_hlines_testing()

Imager::Internal::Hlines
i_int_hlines_new(start_y, count_y, start_x, count_x)
	i_img_dim start_y
	int count_y
	i_img_dim start_x
	int count_x

Imager::Internal::Hlines
i_int_hlines_new_img(im)
	Imager::ImgRaw im

void
i_int_hlines_add(hlines, y, minx, width)
	Imager::Internal::Hlines hlines
	i_img_dim y
	i_img_dim minx
	i_img_dim width

void
i_int_hlines_DESTROY(hlines)
	Imager::Internal::Hlines hlines

SV *
i_int_hlines_dump(hlines)
	Imager::Internal::Hlines hlines

int
i_int_hlines_CLONE_SKIP(cls)

#endif

BOOT:
        PERL_SET_GLOBAL_CALLBACKS;
	PERL_PL_SET_GLOBAL_CALLBACKS;
