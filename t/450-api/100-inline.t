#!perl -w
#
# this tests both the Inline interface and the API
use strict;
use Test::More;
use Imager::Test qw(is_color3 is_color4);
use Config;
eval "require Inline::C;";
plan skip_all => "Inline required for testing API" if $@;

eval "require Parse::RecDescent;";
plan skip_all => "Could not load Parse::RecDescent" if $@;

use Cwd 'getcwd';
plan skip_all => "Inline won't work in directories with spaces"
  if getcwd() =~ / /;

plan skip_all => "perl 5.005_04, 5.005_05 too buggy"
  if $] =~ /^5\.005_0[45]$/;

-d "testout" or mkdir "testout";

print STDERR "Inline version $Inline::VERSION\n";

require Inline;
Inline->import(with => 'Imager');
Inline->import("FORCE"); # force rebuild
Inline->import(C => Config => OPTIMIZE => "-g");

Inline->bind(C => <<'EOS');
#include <math.h>

int pixel_count(Imager::ImgRaw im) {
  return im->xsize * im->ysize;
}

int count_color(Imager::ImgRaw im, Imager::Color c) {
  int count = 0, x, y, chan;
  i_color read_c;

  for (x = 0; x < im->xsize; ++x) {
    for (y = 0; y < im->ysize; ++y) {
      int match = 1;
      i_gpix(im, x, y, &read_c);
      for (chan = 0; chan < im->channels; ++chan) {
        if (read_c.channel[chan] != c->channel[chan]) {
          match = 0;
          break;
        }
      }
      if (match)
        ++count;
    }
  }

  return count;
}

Imager make_10x10() {
  i_img *im = i_img_8_new(10, 10, 3);
  i_color c;
  c.channel[0] = c.channel[1] = c.channel[2] = 255;
  i_box_filled(im, 0, 0, im->xsize-1, im->ysize-1, &c);

  return im;
}

/* tests that all of the APIs are visible - most of them anyway */
Imager do_lots(Imager src) {
  i_img *im = i_img_8_new(100, 100, 3);
  i_img *fill_im = i_img_8_new(5, 5, 3);
  i_img *testim;
  i_color red, blue, green, black, temp_color;
  i_fcolor redf, bluef;
  i_fill_t *hatch, *fhatch_fill;
  i_fill_t *im_fill;
  i_fill_t *solid_fill, *fsolid_fill;
  i_fill_t *fount_fill;
  void *block;
  double matrix[9] = /* 30 degree rotation */
    {
      0.866025,  -0.5,      0, 
      0.5,       0.866025,  0, 
      0,         0,         1,      
    };
  i_fountain_seg fseg;
  i_img_tags tags;
  int entry;
  double temp_double;

  red.channel[0] = 255; red.channel[1] = 0; red.channel[2] = 0;
  red.channel[3] = 255;
  blue.channel[0] = 0; blue.channel[1] = 0; blue.channel[2] = 255;
  blue.channel[3] = 255;
  green.channel[0] = 0; green.channel[1] = 255; green.channel[2] = 0;
  green.channel[3] = 255;
  black.channel[0] = black.channel[1] = black.channel[2] = 0;
  black.channel[3] = 255;
  hatch = i_new_fill_hatch(&red, &blue, 0, 1, NULL, 0, 0);

  i_box(im, 0, 0, 9, 9, &red);
  i_box_filled(im, 10, 0, 19, 9, &blue);
  i_box_cfill(im, 20, 0, 29, 9, hatch);

  /* make an image fill, and try it */
  i_box_cfill(fill_im, 0, 0, 4, 4, hatch);
  im_fill = i_new_fill_image(fill_im, matrix, 2, 2, 0);

  i_box_cfill(im, 30, 0, 39, 9, im_fill);

  /* make a solid fill and try it */
  solid_fill = i_new_fill_solid(&red, 0);
  i_box_cfill(im, 40, 0, 49, 9, solid_fill);

  /* floating fills */
  redf.channel[0] = 1.0; redf.channel[1] = 0; redf.channel[2] = 0;
  redf.channel[3] = 1.0;
  bluef.channel[0] = 0; bluef.channel[1] = 0; bluef.channel[2] = 1.0;
  bluef.channel[3] = 1.0;

  fsolid_fill = i_new_fill_solidf(&redf, 0);
  i_box_cfill(im, 50, 0, 59, 9, fsolid_fill);
 
  fhatch_fill = i_new_fill_hatchf(&redf, &bluef, 0, 2, NULL, 0, 0);
  i_box_cfill(im, 60, 0, 69, 9, fhatch_fill);

  /* fountain fill */
  fseg.start = 0;
  fseg.middle = 0.5;
  fseg.end = 1.0;
  fseg.c[0] = redf;
  fseg.c[1] = bluef;
  fseg.type = i_fst_linear;
  fseg.color = i_fc_hue_down;
  fount_fill = i_new_fill_fount(70, 0, 80, 0, i_ft_linear, i_fr_triangle, 0, i_fts_none, 1, 1, &fseg);

  i_box_cfill(im, 70, 0, 79, 9, fount_fill);

  i_line(im, 0, 10, 10, 15, &blue, 1);
  i_line_aa(im, 0, 19, 10, 15, &red, 1);
  
  i_arc(im, 15, 15, 4, 45, 160, &blue);
  i_arc_aa(im, 25, 15, 4, 75, 280, &red);
  i_arc_cfill(im, 35, 15, 4, 0, 215, hatch);
  i_arc_aa_cfill(im, 45, 15, 4, 30, 210, hatch);
  i_circle_aa(im, 55, 15, 4, &red);
  
  i_box(im, 61, 11, 68, 18, &red);
  i_flood_fill(im, 65, 15, &blue);
  i_box(im, 71, 11, 78, 18, &red);
  i_flood_cfill(im, 75, 15, hatch);

  i_box_filled(im, 1, 21, 9, 24, &red);
  i_box_filled(im, 1, 25, 9, 29, &blue);
  i_flood_fill_border(im, 5, 25, &green, &black);

  i_box_filled(im, 11, 21, 19, 24, &red);
  i_box_filled(im, 11, 25, 19, 29, &blue);
  i_flood_cfill_border(im, 15, 25, hatch, &black);

  {
     double x[3];
     double y[3];
     i_polygon_t poly;
     x[0] = 55;
     y[0] = 25;
     x[1] = 55;
     y[1] = 50;
     x[2] = 70;
     y[2] = 50;
     i_poly_aa_m(im, 3, x, y, i_pfm_evenodd, &red);
     x[2] = 40;
     i_poly_aa_cfill_m(im, 3, x, y, i_pfm_evenodd, hatch);
     y[0] = 65;
     poly.x = x;
     poly.y = y;
     poly.count = 3;
     i_poly_poly_aa(im, 1, &poly, i_pfm_nonzero, &green);
     x[2] = 70;
     i_poly_poly_aa_cfill(im, 1, &poly, i_pfm_nonzero, hatch);
  }

  i_fill_destroy(fount_fill);
  i_fill_destroy(fhatch_fill);
  i_fill_destroy(solid_fill);
  i_fill_destroy(fsolid_fill);
  i_fill_destroy(hatch);
  i_fill_destroy(im_fill);
  i_img_destroy(fill_im);

  /* make sure we can make each image type */
  testim = i_img_16_new(100, 100, 3);
  i_img_destroy(testim);
  testim = i_img_double_new(100, 100, 3);
  i_img_destroy(testim);
  testim = i_img_pal_new(100, 100, 3, 256);
  i_img_destroy(testim);
  testim = i_sametype(im, 50, 50);
  i_img_destroy(testim);
  testim = i_sametype_chans(im, 50, 50, 4);
  i_img_destroy(testim);

  i_clear_error();
  i_push_error(0, "Hello");
  i_push_errorf(0, "%s", "World");

  /* make sure tags create/destroy work */
  i_tags_new(&tags);
  i_tags_destroy(&tags);  

  block = mymalloc(20);
  block = myrealloc(block, 50);
  myfree(block);

  i_tags_set(&im->tags, "lots_string", "foo", -1);
  i_tags_setn(&im->tags, "lots_number", 101);

  if (!i_tags_find(&im->tags, "lots_number", 0, &entry)) {
    i_push_error(0, "lots_number tag not found");
    i_img_destroy(im);
    return NULL;
  }
  i_tags_delete(&im->tags, entry);

  /* these won't delete anything, but it makes sure the macros and function
     pointers are correct */
  i_tags_delbyname(&im->tags, "unknown");
  i_tags_delbycode(&im->tags, 501);
  i_tags_set_float(&im->tags, "lots_float", 0, 3.14);
  if (!i_tags_get_float(&im->tags, "lots_float", 0, &temp_double)) {
    i_push_error(0, "lots_float not found");
    i_img_destroy(im);
    return NULL;
  }
  if (fabs(temp_double - 3.14) > 0.001) {
    i_push_errorf(0, "lots_float incorrect %g", temp_double);
    i_img_destroy(im);
    return NULL;
  }
  i_tags_set_float2(&im->tags, "lots_float2", 0, 100 * sqrt(2.0), 5);
  if (!i_tags_get_int(&im->tags, "lots_float2", 0, &entry)) {
    i_push_error(0, "lots_float2 not found as int");
    i_img_destroy(im);
    return NULL;
  }
  if (entry != 141) { 
    i_push_errorf(0, "lots_float2 unexpected value %d", entry);
    i_img_destroy(im);
    return NULL;
  }

  i_tags_set_color(&im->tags, "lots_color", 0, &red);
  if (!i_tags_get_color(&im->tags, "lots_color", 0, &temp_color)) {
    i_push_error(0, "lots_color not found as color");
    i_img_destroy(im);
    return NULL;
  }

  /* ensure this accepts a NULL pointer */
  i_img_data_release(NULL);

  return im;
}

void
io_fd(int fd) {
  Imager::IO io = io_new_fd(fd);
  i_io_write(io, "test", 4);
  i_io_close(io);
  io_glue_destroy(io);
}

int
io_bufchain_test() {
  Imager::IO io = io_new_bufchain();
  unsigned char *result;
  size_t size;
  if (i_io_write(io, "test2", 5) != 5) {
    fprintf(stderr, "write failed\n");
    return 0;
  }
  if (!i_io_flush(io)) {
    fprintf(stderr, "flush failed\n");
    return 0;
  }
  if (i_io_close(io) != 0) {
    fprintf(stderr, "close failed\n");
    return 0;
  }
  size = io_slurp(io, &result);
  if (size != 5) {
    fprintf(stderr, "wrong size\n");
    return 0;
  }
  if (memcmp(result, "test2", 5)) {
    fprintf(stderr, "data mismatch\n");
    return 0;
  }
  if (i_io_seek(io, 0, 0) != 0) {
    fprintf(stderr, "seek failure\n");
    return 0;
  }
  myfree(result);
  io_glue_destroy(io);

  return 1;
}

const char *
io_buffer_test(SV *in) {
  STRLEN len;
  const char *in_str = SvPV(in, len);
  static char buf[100];
  Imager::IO io = io_new_buffer(in_str, len, NULL, NULL);
  ssize_t read_size;

  read_size = i_io_read(io, buf, sizeof(buf)-1);
  io_glue_destroy(io);
  if (read_size < 0 || read_size >= sizeof(buf)) {
    return "";
  }

  buf[read_size] = '\0';

  return buf;
}

const char *
io_peekn_test(SV *in) {
  STRLEN len;
  const char *in_str = SvPV(in, len);
  static char buf[100];
  Imager::IO io = io_new_buffer(in_str, len, NULL, NULL);
  ssize_t read_size;

  read_size = i_io_peekn(io, buf, sizeof(buf)-1);
  io_glue_destroy(io);
  if (read_size < 0 || read_size >= sizeof(buf)) {
    return "";
  }

  buf[read_size] = '\0';

  return buf;
}

const char *
io_gets_test(SV *in) {
  STRLEN len;
  const char *in_str = SvPV(in, len);
  static char buf[100];
  Imager::IO io = io_new_buffer(in_str, len, NULL, NULL);
  ssize_t read_size;

  read_size = i_io_gets(io, buf, sizeof(buf), 's');
  io_glue_destroy(io);
  if (read_size < 0 || read_size >= sizeof(buf)) {
    return "";
  }

  return buf;
}

int
io_getc_test(SV *in) {
  STRLEN len;
  const char *in_str = SvPV(in, len);
  static char buf[100];
  Imager::IO io = io_new_buffer(in_str, len, NULL, NULL);
  int result;

  result = i_io_getc(io);
  io_glue_destroy(io);

  return result;
}

int
io_peekc_test(SV *in) {
  STRLEN len;
  const char *in_str = SvPV(in, len);
  static char buf[100];
  Imager::IO io = io_new_buffer(in_str, len, NULL, NULL);
  int result;

  i_io_set_buffered(io, 0);

  result = i_io_peekc(io);
  io_glue_destroy(io);

  return result;
}



int
test_render_color(Imager work_8) {
  i_render *r8;
  i_color c;
  unsigned char render_coverage[3];

  render_coverage[0] = 0;
  render_coverage[1] = 128;
  render_coverage[2] = 255;

  r8 = i_render_new(work_8, 10);
  c.channel[0] = 128;
  c.channel[1] = 255;
  c.channel[2] = 0;
  c.channel[3] = 255;
  i_render_color(r8, 0, 0, sizeof(render_coverage), render_coverage, &c);

  c.channel[3] = 128;
  i_render_color(r8, 0, 1, sizeof(render_coverage), render_coverage, &c);

  c.channel[3] = 0;
  i_render_color(r8, 0, 2, sizeof(render_coverage), render_coverage, &c);

  i_render_delete(r8);

  return 1;
}

int
raw_psamp(Imager im, int chan_count) {
  static i_sample_t samps[] = { 0, 127, 255 };

  i_clear_error();
  return i_psamp(im, 0, 1, 0, samps, NULL, chan_count);
}

int
raw_psampf(Imager im, int chan_count) {
  static i_fsample_t samps[] = { 0, 0.5, 1.0 };

  i_clear_error();
  return i_psampf(im, 0, 1, 0, samps, NULL, chan_count);
}

int
test_mutex() {
  i_mutex_t m;

  m = i_mutex_new();
  i_mutex_lock(m);
  i_mutex_unlock(m);
  i_mutex_destroy(m);

  return 1;
}

int
test_slots() {
  im_slot_t slot = im_context_slot_new(NULL);

  if (im_context_slot_get(aIMCTX, slot)) {
    fprintf(stderr, "slots should default to NULL\n");
    return 0;
  }
  if (!im_context_slot_set(aIMCTX, slot, &slot)) {
    fprintf(stderr, "set slot failed\n");
    return 0;
  }

  if (im_context_slot_get(aIMCTX, slot) != &slot) {
    fprintf(stderr, "get slot didn't match\n");
    return 0;
  }

  return 1;
}

int
color_channels(Imager im) {
  return i_img_color_channels(im);
}

int
color_model(Imager im) {
  return (int)i_img_color_model(im);
}

int
alpha_channel(Imager im) {
  int channel;
  if (!i_img_alpha_channel(im, &channel))
    channel = -1;

  return channel;
}

int
test_map_mem(Imager::IO io) {
  const void *p;
  size_t size;
  int ok = 1;

  if (i_io_mmap(io, &p, &size)) {
    if (memcmp(p, "testdata", 8) != 0) {
      fprintf(stderr, "mapped buffer doesn't match expected");
      ok = 0;
    }
    if (!i_io_munmap(io)) {
      fprintf(stderr, "Failed to munmap memory buffer");
      ok = 0;
    }
  }
  else {
    fprintf(stderr, "Failed to mmap memory buffer");
    ok = 0;
  }

  return ok;
}

int
test_map_fd(Imager::IO io, SV *sv) {
  const void *p;
  size_t size;
  int ok = 1;
  const char *check;
  STRLEN len;

  check = SvPV(sv, len);
  if (i_io_mmap(io, &p, &size)) {
    if (memcmp(p, check, len) != 0) {
      fprintf(stderr, "mapped buffer doesn't match expected\n");
      ok = 0;
    }
    if (!i_io_munmap(io)) {
      fprintf(stderr, "Failed to munmap memory buffer\n");
      ok = 0;
    }
  }
  else {
    fprintf(stderr, "Failed to mmap memory buffer\n");
    ok = 0;
  }

  if (ok) {
    size_t maxmmap = i_io_get_max_mmap_size();
    if (!(ok = i_io_set_max_mmap_size(10))) {
      fprintf(stderr, "i_io_set_max_mmap_size(10) failed\n");
    }
    if (ok) {
      if (!(ok = !i_io_mmap(io, &p, &len))) {
        fprintf(stderr, "i_io_mmap() succeeded on too large a file\n");
      }
    }
    i_io_set_max_mmap_size(maxmmap);
  }

  return ok;
}

int
test_refcount(Imager im) {
  i_img_refcnt_inc(im);
  i_img_destroy(im);
  return i_img_refcnt(im);
}

int
check_image(Imager im) {
  return i_img_check_entries(im);
}

/* source image just used to grab a vtable */
int
new_image(Imager im) {
  i_img *im2 = i_img_new(im->vtbl, 10, 10, 3, 0, i_8_bits);
  im2->bytes = 10 * 10 * 3;
  im2->idata = mymalloc(im2->bytes);
  i_img_init(im2);
  i_img_destroy(im2);
  return 1;
}

Imager
new_8_extra(i_img_dim x, i_img_dim y, int channels, int extra) {
  return i_img_8_new_extra(x, y, channels, extra);
}

Imager
new_16_extra(i_img_dim x, i_img_dim y, int channels, int extra) {
  return i_img_16_new_extra(x, y, channels, extra);
}

Imager
new_double_extra(i_img_dim x, i_img_dim y, int channels, int extra) {
  return i_img_double_new_extra(x, y, channels, extra);
}

int
test_data_alloc(Imager im) {
  i_image_data_alloc_t *alloc;
  void *somedata = mymalloc(100);
  int start_refcnt = i_img_refcnt(im);
  alloc = i_new_image_data_alloc_free(im, somedata);
  /* yes, failing the test leaks */
  if (!alloc) {
    fprintf(stderr, "no object from i_new_image_data_alloc_free\n");
    return 0;
  }
  i_img_data_release(alloc);
  alloc = i_new_image_data_alloc_def(im);
  if (!alloc) {
    fprintf(stderr, "no object from i_new_image_data_alloc_def\n");
    return 0;
  }
  if (i_img_refcnt(im) == start_refcnt) {
    fprintf(stderr, "no change in image refcnt from i_new_image_data_alloc_def\n");
    return 0;
  }
  i_img_data_release(alloc);
  if (i_img_refcnt(im) != start_refcnt) {
    fprintf(stderr, "refcnt not restored after release\n");
    return 0;
  }
  return 1;
}

int
test_zmalloc() {
  void *data = mymalloc(100);
  myfree(data);
  data = myzmalloc_file_line(100, __FILE__, __LINE__);
  myfree(data);
  return 1;
}

static
i_img_dim
my_gsamp(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samps,
              const int *chans, int chan_count) {
  int ch;
  i_img_dim count, i, w;
  unsigned char *data;
  unsigned totalch = im->channels + im->extrachannels;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= totalch) {
          i_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return 0;
        }
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = data[chans[ch]];
          ++count;
        }
        data += totalch;
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	i_push_errorf(0, "chan_count %d out of range, must be >0, <= total channels", 
		      chan_count);
	return 0;
      }
      for (i = 0; i < w; ++i) {
        for (ch = 0; ch < chan_count; ++ch) {
          *samps++ = data[ch];
          ++count;
        }
        data += totalch;
      }
    }

    return count;
  }
  else {
    return 0;
  }
}

static
i_img_dim
my_psamp(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, 
	  const i_sample_t *samps, const int *chans, int chan_count) {
  int ch;
  i_img_dim count, i, w;
  unsigned char *data;
  unsigned totalch = im->channels + im->extrachannels;

  if (y >=0 && y < im->ysize && l < im->xsize && l >= 0) {
    if (r > im->xsize)
      r = im->xsize;
    data = im->idata + (l+y*im->xsize) * totalch;
    w = r - l;
    count = 0;

    if (chans) {
      /* make sure we have good channel numbers */
      /* and test if all channels specified are in the mask */
      int all_in_mask = 1;
      for (ch = 0; ch < chan_count; ++ch) {
        if (chans[ch] < 0 || chans[ch] >= totalch) {
          i_push_errorf(aIMCTX, 0, "No channel %d in this image", chans[ch]);
          return -1;
        }
	if (!((1 << chans[ch]) & im->ch_mask))
	  all_in_mask = 0;
      }
      if (all_in_mask) {
	for (i = 0; i < w; ++i) {
	  for (ch = 0; ch < chan_count; ++ch) {
	    data[chans[ch]] = *samps++;
	    ++count;
	  }
	  data += totalch;
	}
      }
      else {
	for (i = 0; i < w; ++i) {
	  for (ch = 0; ch < chan_count; ++ch) {
	    if (im->ch_mask & (1 << (chans[ch])))
	      data[chans[ch]] = *samps;
	    ++samps;
	    ++count;
	  }
	  data += totalch;
	}
      }
    }
    else {
      if (chan_count <= 0 || chan_count > totalch) {
	i_push_errorf(0, "chan_count %d out of range, must be >0, <= channels", 
		      chan_count);
	return -1;
      }
      for (i = 0; i < w; ++i) {
	unsigned mask = 1;
        for (ch = 0; ch < chan_count; ++ch) {
	  if (im->ch_mask & mask)
	    data[ch] = *samps;
	  ++samps;
          ++count;
	  mask <<= 1;
        }
        data += totalch;
      }
    }

    return count;
  }
  else {
    i_push_error(0, "Image position outside of image");
    return -1;
  }
}

static const i_img_vtable *
make_vtbl(void) {
  static i_img_vtable my_vtbl;
  static int made_vtbl;
  if (!made_vtbl) {
    i_img_vtable init =
      {
      IMAGER_API_LEVEL,
      my_gsamp,
      i_gsampf_fp,
      my_psamp,
      i_psampf_fp,
      NULL,
      i_gsamp_bits_fb,
      i_psamp_bits_fb,
      i_img_data_fallback,
      NULL, /* i_f_imageop */

      NULL, /* i_f_gpal */
      NULL, /* i_f_ppal */
      NULL, /* i_f_addcolors */
      NULL, /* i_f_getcolors */
      NULL, /* i_f_colorcount */
      NULL, /* i_f_maxcolors */
      NULL, /* i_f_findcolor */
      NULL /* i_f_setcolors */
    };
    ++made_vtbl;
    my_vtbl = init;
  }
  return &my_vtbl;
}

Imager
make_my_image(i_img_dim xsize, i_img_dim ysize, int channels) {
  size_t bytes;
  i_img *im;
  if (channels < 1 || channels > MAXCHANNELS) {
    i_push_error(0, "Invalid channels");
    return NULL;
  }
  if (xsize < 1 || ysize < 1) {
    i_push_error(0, "Image must have pixels");
    return NULL;
  }
  bytes = (size_t)xsize * (size_t)ysize * (size_t)channels;
  if (bytes / (size_t)xsize / (size_t) channels != (size_t)ysize) {
    i_push_error(0, "Integer overflow calculating image allocation");
    return NULL;
  }

  im = i_img_new(make_vtbl(), xsize, ysize, channels, 0, i_8_bits);
  if (im == NULL) {
    return NULL;
  }
  im->bytes = bytes;
  im->idata = myzmalloc(im->bytes);

  i_img_init(im);
  return im;
}


int
test_forwarders() {
  int ok = 1;

#define STR(x) #x
#define TEST_FORWARD(x) \
  if (x == NULL) {        \
    ok = 0;               \
    fprintf(stderr, "forward pointer %s is NULL\n", STR(x));  \
  }

  TEST_FORWARD(i_addcolors_forward);
  TEST_FORWARD(i_getcolors_forward);
  TEST_FORWARD(i_setcolors_forward);
  TEST_FORWARD(i_colorcount_forward);
  TEST_FORWARD(i_maxcolors_forward);
  TEST_FORWARD(i_findcolor_forward);

  return ok;
}

EOS

my $im = Imager->new(xsize=>50, ysize=>50);
is(pixel_count($im), 2500, "pixel_count");

my $black = Imager::Color->new(0,0,0);
is(count_color($im, $black), 2500, "count_color black on black image");

my $im2 = make_10x10();
my $white = Imager::Color->new(255, 255, 255);
is(count_color($im2, $white), 100, "check new image white count");
ok($im2->box(filled=>1, xmin=>1, ymin=>1, xmax => 8, ymax=>8, color=>$black),
   "try new image");
is(count_color($im2, $black), 64, "check modified black count");
is(count_color($im2, $white), 36, "check modified white count");

my $im3 = do_lots($im2);
ok($im3, "do_lots()")
  or print "# ", Imager->_error_as_msg, "\n";
ok($im3->write(file=>'testout/t82lots.ppm'), "write t82lots.ppm");

{ # RT #24992
  # the T_IMAGER_FULL_IMAGE typemap entry was returning a blessed
  # hash with an extra ref, causing memory leaks

  my $im = make_10x10();
  my $im2 = Imager->new(xsize => 10, ysize => 10);
  require B;
  my $imb = B::svref_2object($im);
  my $im2b = B::svref_2object($im2);
  is ($imb->REFCNT, $im2b->REFCNT, 
      "check refcnt of imager object hash between normal and typemap generated");
}

SKIP:
{
  my $fd_filename = "testout/t82fd.txt";
  {
    open my $fh, ">",$fd_filename
      or skip("Can't create file: $!", 1);
    io_fd(fileno($fh));
    $fh->close;
  }
  {
    open my $fh, "<", $fd_filename
      or skip("Can't open file: $!", 1);
    my $data = <$fh>;
    is($data, "test", "make sure data written to fd");
  }
  unlink $fd_filename;
}

ok(io_bufchain_test(), "check bufchain functions");

is(io_buffer_test("test3"), "test3", "check io_new_buffer() and i_io_read");

is(io_peekn_test("test5"), "test5", "check i_io_peekn");

is(io_gets_test("test"), "tes", "check i_io_gets()");

is(io_getc_test("ABC"), ord "A", "check i_io_getc(_imp)?");

is(io_getc_test("XYZ"), ord "X", "check i_io_peekc(_imp)?");

for my $bits (8, 16) {
  print "# bits: $bits\n";

  # the floating point processing is a little more accurate
  my $bump = $bits == 16 ? 1 : 0;
  {
    my $im = Imager->new(xsize => 10, ysize => 10, channels => 3, bits => $bits);
    ok($im->box(filled => 1, color => '#808080'), "fill work image with gray");
    ok(test_render_color($im),
       "call render_color on 3 channel image");
    is_color3($im->getpixel(x => 0, y => 0), 128, 128, 128,
	      "check zero coverage, alpha 255 color, bits $bits");
    is_color3($im->getpixel(x => 1, y => 0), 128, 191+$bump, 63+$bump,
	      "check 128 coverage, alpha 255 color, bits $bits");
    is_color3($im->getpixel(x => 2, y => 0), 128, 255, 0,
	      "check 255 coverage, alpha 255 color, bits $bits");

    is_color3($im->getpixel(x => 0, y => 1), 128, 128, 128,
	      "check zero coverage, alpha 128 color, bits $bits");
    is_color3($im->getpixel(x => 1, y => 1), 128, 159+$bump, 95+$bump,
	      "check 128 coverage, alpha 128 color, bits $bits");
    is_color3($im->getpixel(x => 2, y => 1), 128, 191+$bump, 63+$bump,
	      "check 255 coverage, alpha 128 color, bits $bits");

    is_color3($im->getpixel(x => 0, y => 2), 128, 128, 128,
	      "check zero coverage, alpha 0 color, bits $bits");
    is_color3($im->getpixel(x => 1, y => 2), 128, 128, 128,
	      "check 128 coverage, alpha 0 color, bits $bits");
    is_color3($im->getpixel(x => 2, y => 2), 128, 128, 128,
	      "check 255 coverage, alpha 0 color, bits $bits");
  }
  {
    my $im = Imager->new(xsize => 10, ysize => 10, channels => 4, bits => $bits);
    ok($im->box(filled => 1, color => '#808080'), "fill work image with opaque gray");
    ok(test_render_color($im),
       "call render_color on 4 channel image");
    is_color4($im->getpixel(x => 0, y => 0), 128, 128, 128, 255,
	      "check zero coverage, alpha 255 color, bits $bits");
    is_color4($im->getpixel(x => 1, y => 0), 128, 191+$bump, 63+$bump, 255,
	      "check 128 coverage, alpha 255 color, bits $bits");
    is_color4($im->getpixel(x => 2, y => 0), 128, 255, 0, 255,
	      "check 255 coverage, alpha 255 color, bits $bits");

    is_color4($im->getpixel(x => 0, y => 1), 128, 128, 128, 255,
	      "check zero coverage, alpha 128 color, bits $bits");
    is_color4($im->getpixel(x => 1, y => 1), 128, 159+$bump, 95+$bump, 255,
	      "check 128 coverage, alpha 128 color, bits $bits");
    is_color4($im->getpixel(x => 2, y => 1), 128, 191+$bump, 63+$bump, 255,
	      "check 255 coverage, alpha 128 color, bits $bits");

    is_color4($im->getpixel(x => 0, y => 2), 128, 128, 128, 255,
	      "check zero coverage, alpha 0 color, bits $bits");
    is_color4($im->getpixel(x => 1, y => 2), 128, 128, 128, 255,
	      "check 128 coverage, alpha 0 color, bits $bits");
    is_color4($im->getpixel(x => 2, y => 2), 128, 128, 128, 255,
	      "check 255 coverage, alpha 0 color, bits $bits");
  }

  {
    my $im = Imager->new(xsize => 10, ysize => 10, channels => 4, bits => $bits);
    ok($im->box(filled => 1, color => Imager::Color->new(128, 128, 128, 64)), "fill work image with translucent gray");
    ok(test_render_color($im),
       "call render_color on 4 channel image");
    is_color4($im->getpixel(x => 0, y => 0), 128, 128, 128, 64,
	      "check zero coverage, alpha 255 color, bits $bits");
    is_color4($im->getpixel(x => 1, y => 0), 128, 230, 25+$bump, 159+$bump,
	      "check 128 coverage, alpha 255 color, bits $bits");
    is_color4($im->getpixel(x => 2, y => 0), 128, 255, 0, 255,
	      "check 255 coverage, alpha 255 color, bits $bits");

    is_color4($im->getpixel(x => 0, y => 1), 128, 128, 128, 64,
	      "check zero coverage, alpha 128 color, bits $bits");
    is_color4($im->getpixel(x => 1, y => 1), 129-$bump, 202-$bump, 55, 111+$bump,
	      "check 128 coverage, alpha 128 color, bits $bits");
    is_color4($im->getpixel(x => 2, y => 1), 128, 230, 25+$bump, 159+$bump,
	      "check 255 coverage, alpha 128 color, bits $bits");

    is_color4($im->getpixel(x => 0, y => 2), 128, 128, 128, 64,
	      "check zero coverage, alpha 0 color, bits $bits");
    is_color4($im->getpixel(x => 1, y => 2), 128, 128, 128, 64,
	      "check 128 coverage, alpha 0 color, bits $bits");
    is_color4($im->getpixel(x => 2, y => 2), 128, 128, 128, 64,
	      "check 255 coverage, alpha 0 color, bits $bits");
  }
}

{
  my $im = Imager->new(xsize => 10, ysize => 10);
  is(raw_psamp($im, 4), -1, "bad channel list (4) for psamp should fail");
  is(_get_error(), "chan_count 4 out of range, must be >0, <= channels",
     "check message");
  is(raw_psamp($im, 0), -1, "bad channel list (0) for psamp should fail");
  is(_get_error(), "chan_count 0 out of range, must be >0, <= channels",
     "check message");
  is(raw_psampf($im, 4), -1, "bad channel list (4) for psampf should fail");
  is(_get_error(), "chan_count 4 out of range, must be >0, <= total channels",
     "check message");
  is(raw_psampf($im, 0), -1, "bad channel list (0) for psampf should fail");
  is(_get_error(), "chan_count 0 out of range, must be >0, <= total channels",
     "check message");
}

{
  my $im = Imager->new(xsize => 10, ysize => 10, bits => 16);
  is(raw_psamp($im, 4), -1, "bad channel list (4) for psamp should fail (16-bit)");
  is(_get_error(), "chan_count 4 out of range, must be >0, <= channels",
     "check message");
  is(raw_psamp($im, 0), -1, "bad channel list (0) for psamp should fail (16-bit)");
  is(_get_error(), "chan_count 0 out of range, must be >0, <= channels",
     "check message");
  is(raw_psampf($im, 4), -1, "bad channel list (4) for psampf should fail (16-bit)");
  is(_get_error(), "chan_count 4 out of range, must be >0, <= channels",
     "check message");
  is(raw_psampf($im, 0), -1, "bad channel list (0) for psampf should fail (16-bit)");
  is(_get_error(), "chan_count 0 out of range, must be >0, <= channels",
     "check message");
}

{
  my $im = Imager->new(xsize => 10, ysize => 10, bits => 'double');
  is(raw_psamp($im, 4), -1, "bad channel list (4) for psamp should fail (double)");
  is(_get_error(), "chan_count 4 out of range, must be >0, <= total channels",
     "check message");
  is(raw_psamp($im, 0), -1,, "bad channel list (0) for psamp should fail (double)");
  is(_get_error(), "chan_count 0 out of range, must be >0, <= total channels",
     "check message");
  is(raw_psampf($im, 4), -1, "bad channel list (4) for psampf should fail (double)");
  is(_get_error(), "chan_count 4 out of range, must be >0, <= total channels",
     "check message");
  is(raw_psampf($im, 0), -1, "bad channel list (0) for psampf should fail (double)");
  is(_get_error(), "chan_count 0 out of range, must be >0, <= total channels",
     "check message");
}

{
  my $im = Imager->new(xsize => 10, ysize => 10, type => "paletted");
  is(raw_psamp($im, 4), -1, "bad channel list (4) for psamp should fail (paletted)");
  is(_get_error(), "chan_count 4 out of range, must be >0, <= channels",
     "check message");
  is(raw_psamp($im, 0), -1, "bad channel list (0) for psamp should fail (paletted)");
  is(_get_error(), "chan_count 0 out of range, must be >0, <= channels",
     "check message");
  is(raw_psampf($im, 4), -1, "bad channel list (4) for psampf should fail (paletted)");
  is(_get_error(), "chan_count 4 out of range, must be >0, <= channels",
     "check message");
  is(raw_psampf($im, 0), -1, "bad channel list (0) for psampf should fail (paletted)");
  is(_get_error(), "chan_count 0 out of range, must be >0, <= channels",
     "check message");
  is($im->type, "paletted", "make sure we kept the image type");
}

{
  my $rgb = Imager->new(xsize => 10, ysize => 10);
  is(color_model($rgb), 3, "check i_img_color_model() api");
  is(color_channels($rgb), 3, "check i_img_color_channels() api");
  is(alpha_channel($rgb), -1, "check i_img_alpha_channel() api");
}

ok(test_mutex(), "call mutex APIs");

ok(test_slots(), "call slot APIs");

{
  my $s = "testdata";
  my $io = Imager::IO->new_buffer($s);
  ok(test_map_mem($io), "check we can map memory IO");
}

SKIP:
{
  $Config{d_mmap} && $Config{d_munmap}
    or skip "No mmap available", 1;
  open my $fh, "<", $0 or skip "cannot open $0", 1;
  binmode $fh;
  my $buf;
  read($fh, $buf, 8);
  my $io = Imager::IO->new_fd(fileno($fh));
  ok(test_map_fd($io, $buf), "test we can map fd io");
}

{
  my $im = Imager->new(xsize => 10, ysize => 12);
  is(test_refcount($im), 1, "check refcounts visible to API");
}

{
  my $im = Imager->new(xsize => 10, ysize => 12);
  ok(check_image($im), "check i_img_check_entries visible");
}

{
  my $im = Imager->new(xsize => 10, ysize => 12);
  ok(new_image($im), "check i_img_new() visible in API");
}

{
  my $im = new_8_extra(10, 12, 3, 4);
  ok($im, "make 8-bit extra channel image");
  is($im->bits, 8, "got an 8 bit image");
  is($im->extrachannels, 4, "got the extra channels");
}
{
  my $im = new_16_extra(10, 12, 3, 4);
  ok($im, "make 16-bit extra channel image");
  is($im->bits, 16, "got an 16 bit image");
  is($im->extrachannels, 4, "got the extra channels");
}
{
  my $im = new_double_extra(10, 12, 3, 4);
  ok($im, "make double extra channel image");
  is($im->bits, "double", "got an double image");
  is($im->extrachannels, 4, "got the extra channels");
}

{
  my $im = Imager->new(xsize => 100, ysize => 100);
  ok(test_data_alloc($im), "test data alloc apis visible");
}

ok(test_zmalloc(), "calls to myzmalloc");

{
  my $im = make_my_image(21, 31, 3);
  ok($im, "make custom image");
  is($im->bits, 8, "and it's 8 bits");
  ok($im->setpixel(x => 0, y => 0, color => "#F00"),
     "set a pixel");
  my @fsamps = $im->getsamples(y => 0, type => "float", width => 1);
  is_deeply(\@fsamps, [ 1, 0, 0 ], "try gsampf");
  is($im->setsamples(data => [ 0, 1, 0 ], y => 5, x => 6, type => "float"), 3,
     "try psampf");
  is_deeply([ $im->getsamples(y => 5, x => 6, width => 1, type => "9bit") ],
            [ 0, 511, 0 ], "gsamp_bits");
  is($im->setsamples(data => [ 0, 1023, 1023 ], x => 7, y => 4, type => "10bit"),
     3, "psamp_bits");
  ok($im->data(flags => "synth"), "check we can fetch data");
}

ok(test_forwarders(), "test paletted forwarders are set");

=item APIs to add TODO

i_img_data_fallback<
i_gsamp_bits_fb
i_psamp_bits_fb
i_gsampf_fp

i_addcolors_forward
i_getcolors_forward
i_setcolors_forward
i_colorcount_forward
i_maxcolors_forward
i_findcolor_forward

=cut

done_testing();

sub _get_error {
  my @errors = Imager::i_errors();
  return join(": ", map $_->[0], @errors);
}
