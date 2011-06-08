#include "imager.h"
#include "log.h"
#include "iolayer.h"
#include "imageri.h"

#include <stdlib.h>
#include <errno.h>


/*
=head1 NAME

pnm.c - implements reading and writing ppm/pnm/pbm files, uses io layer.

=head1 SYNOPSIS

   io_glue *ig = io_new_fd( fd );
   i_img *im   = i_readpnm_wiol(ig, 0); // no limit on how much is read
   // or 
   io_glue *ig = io_new_fd( fd );
   return_code = i_writepnm_wiol(im, ig); 

=head1 DESCRIPTION

pnm.c implements the basic functions to read and write portable 
anymap files.  It uses the iolayer and needs either a seekable source
or an entire memory mapped buffer.

=head1 FUNCTION REFERENCE

Some of these functions are internal.

=over

=cut
*/


#define BSIZ 1024
#define misspace(x) (x==' ' || x=='\n' || x=='\r' || x=='\t' || x=='\f' || x=='\v')
#define misnumber(x) (x <= '9' && x>='0')

static char *typenames[]={"ascii pbm", "ascii pgm", "ascii ppm", "binary pbm", "binary pgm", "binary ppm"};

/*
 * Type to encapsulate the local buffer
 * management skipping over in a file 
 */

typedef struct {
  io_glue *ig;
  int len;
  int cp;
  char buf[BSIZ];
} mbuf;


static
void init_buf(mbuf *mb, io_glue *ig) {
  mb->len = 0;
  mb->cp  = 0;
  mb->ig  = ig;
}



/*
=item gnext(mbuf *mb)

Fetches a character and advances in stream by one character.  
Returns a pointer to the byte or NULL on failure (internal).

   mb - buffer object

=cut
*/

#define gnext(mb) (((mb)->cp == (mb)->len) ? gnextf(mb) : (mb)->buf + (mb)->cp++)

static
char *
gnextf(mbuf *mb) {
  io_glue *ig = mb->ig;
  if (mb->cp == mb->len) {
    mb->cp = 0;
    mb->len = ig->readcb(ig, mb->buf, BSIZ);
    if (mb->len == -1) {
      i_push_error(errno, "file read error");
      mm_log((1, "i_readpnm: read error\n"));
      return NULL;
    }
    if (mb->len == 0) {
      mm_log((1, "i_readpnm: end of file\n"));
      return NULL;
    }
  }
  return &mb->buf[mb->cp++];
}


/*
=item gpeek(mbuf *mb)

Fetches a character but does NOT advance.  Returns a pointer to
the byte or NULL on failure (internal).

   mb - buffer object

=cut
*/

#define gpeek(mb) ((mb)->cp == (mb)->len ? gpeekf(mb) : (mb)->buf + (mb)->cp)

static
char *
gpeekf(mbuf *mb) {
  io_glue *ig = mb->ig;
  if (mb->cp == mb->len) {
    mb->cp = 0;
    mb->len = ig->readcb(ig, mb->buf, BSIZ);
    if (mb->len == -1) {
      i_push_error(errno, "read error");
      mm_log((1, "i_readpnm: read error\n"));
      return NULL;
    }
    if (mb->len == 0) {
      mm_log((1, "i_readpnm: end of file\n"));
      return NULL;
    }
  }
  return &mb->buf[mb->cp];
}

int
gread(mbuf *mb, unsigned char *buf, size_t read_size) {
  int total_read = 0;
  if (mb->cp != mb->len) {
    int avail_size = mb->len - mb->cp;
    int use_size = read_size > avail_size ? avail_size : read_size;
    memcpy(buf, mb->buf+mb->cp, use_size);
    mb->cp += use_size;
    total_read += use_size;
    read_size -= use_size;
    buf += use_size;
  }
  if (read_size) {
    io_glue *ig = mb->ig;
    int read_res = i_io_read(ig, buf, read_size);
    if (read_res >= 0) {
      total_read += read_res;
    }
  }
  return total_read;
}


/*
=item skip_spaces(mb)

Advances in stream until it is positioned at a
non white space character. (internal)

   mb - buffer object

=cut
*/

static
int
skip_spaces(mbuf *mb) {
  char *cp;
  while( (cp = gpeek(mb)) && misspace(*cp) ) if ( !gnext(mb) ) break;
  if (!cp) return 0;
  return 1;
}


/*
=item skip_comment(mb)

Advances in stream over whitespace and a comment if one is found. (internal)

   mb - buffer object

=cut
*/

static
int
skip_comment(mbuf *mb) {
  char *cp;

  if (!skip_spaces(mb)) return 0;

  if (!(cp = gpeek(mb))) return 0;
  if (*cp == '#') {
    while( (cp = gpeek(mb)) && (*cp != '\n' && *cp != '\r') ) {
      if ( !gnext(mb) ) break;
    }
  }
  if (!cp) return 0;
  
  return 1;
}


/*
=item gnum(mb, i)

Fetches the next number from stream and stores in i, returns true
on success else false.

   mb - buffer object
   i  - integer to store result in

=cut
*/

static
int
gnum(mbuf *mb, int *i) {
  char *cp;
  *i = 0;

  if (!skip_spaces(mb)) return 0; 

  if (!(cp = gpeek(mb))) 
    return 0;
  if (!misnumber(*cp))
    return 0;
  while( (cp = gpeek(mb)) && misnumber(*cp) ) {
    *i = *i*10+(*cp-'0');
    cp = gnext(mb);
  }
  return 1;
}

static
i_img *
read_pgm_ppm_bin8(mbuf *mb, i_img *im, int width, int height, 
                  int channels, int maxval, int allow_incomplete) {
  i_color *line, *linep;
  int read_size;
  unsigned char *read_buf, *readp;
  int x, y, ch;
  int rounder = maxval / 2;

  line = mymalloc(width * sizeof(i_color));
  read_size = channels * width;
  read_buf = mymalloc(read_size);
  for(y=0;y<height;y++) {
    linep = line;
    readp = read_buf;
    if (gread(mb, read_buf, read_size) != read_size) {
      myfree(line);
      myfree(read_buf);
      if (allow_incomplete) {
        i_tags_setn(&im->tags, "i_incomplete", 1);
        i_tags_setn(&im->tags, "i_lines_read", y);
        return im;
      }
      else {
        i_push_error(0, "short read - file truncated?");
        i_img_destroy(im);
        return NULL;
      }
    }
    if (maxval == 255) {
      for(x=0; x<width; x++) {
        for(ch=0; ch<channels; ch++) {
          linep->channel[ch] = *readp++;
        }
        ++linep;
      }
    }
    else {
      for(x=0; x<width; x++) {
        for(ch=0; ch<channels; ch++) {
          /* we just clamp samples to the correct range */
          unsigned sample = *readp++;
          if (sample > maxval)
            sample = maxval;
          linep->channel[ch] = (sample * 255 + rounder) / maxval;
        }
        ++linep;
      }
    }
    i_plin(im, 0, width, y, line);
  }
  myfree(read_buf);
  myfree(line);

  return im;
}

static
i_img *
read_pgm_ppm_bin16(mbuf *mb, i_img *im, int width, int height, 
                  int channels, int maxval, int allow_incomplete) {
  i_fcolor *line, *linep;
  int read_size;
  unsigned char *read_buf, *readp;
  int x, y, ch;
  double maxvalf = maxval;

  line = mymalloc(width * sizeof(i_fcolor));
  read_size = channels * width * 2;
  read_buf = mymalloc(read_size);
  for(y=0;y<height;y++) {
    linep = line;
    readp = read_buf;
    if (gread(mb, read_buf, read_size) != read_size) {
      myfree(line);
      myfree(read_buf);
      if (allow_incomplete) {
        i_tags_setn(&im->tags, "i_incomplete", 1);
        i_tags_setn(&im->tags, "i_lines_read", y);
        return im;
      }
      else {
        i_push_error(0, "short read - file truncated?");
        i_img_destroy(im);
        return NULL;
      }
    }
    for(x=0; x<width; x++) {
      for(ch=0; ch<channels; ch++) {
        unsigned sample = (readp[0] << 8) + readp[1];
        if (sample > maxval)
          sample = maxval;
        readp += 2;
        linep->channel[ch] = sample / maxvalf;
      }
      ++linep;
    }
    i_plinf(im, 0, width, y, line);
  }
  myfree(read_buf);
  myfree(line);

  return im;
}

static 
i_img *
read_pbm_bin(mbuf *mb, i_img *im, int width, int height, int allow_incomplete) {
  i_palidx *line, *linep;
  int read_size;
  unsigned char *read_buf, *readp;
  int x, y;
  unsigned mask;

  line = mymalloc(width * sizeof(i_palidx));
  read_size = (width + 7) / 8;
  read_buf = mymalloc(read_size);
  for(y = 0; y < height; y++) {
    if (gread(mb, read_buf, read_size) != read_size) {
      myfree(line);
      myfree(read_buf);
      if (allow_incomplete) {
        i_tags_setn(&im->tags, "i_incomplete", 1);
        i_tags_setn(&im->tags, "i_lines_read", y);
        return im;
      }
      else {
        i_push_error(0, "short read - file truncated?");
        i_img_destroy(im);
        return NULL;
      }
    }
    linep = line;
    readp = read_buf;
    mask = 0x80;
    for(x = 0; x < width; ++x) {
      *linep++ = *readp & mask ? 1 : 0;
      mask >>= 1;
      if (mask == 0) {
        ++readp;
        mask = 0x80;
      }
    }
    i_ppal(im, 0, width, y, line);
  }
  myfree(read_buf);
  myfree(line);

  return im;
}

/* unlike pgm/ppm pbm:
  - doesn't require spaces between samples (bits)
  - 1 (maxval) is black instead of white
*/
static 
i_img *
read_pbm_ascii(mbuf *mb, i_img *im, int width, int height, int allow_incomplete) {
  i_palidx *line, *linep;
  int x, y;

  line = mymalloc(width * sizeof(i_palidx));
  for(y = 0; y < height; y++) {
    linep = line;
    for(x = 0; x < width; ++x) {
      char *cp;
      skip_spaces(mb);
      if (!(cp = gnext(mb)) || (*cp != '0' && *cp != '1')) {
        myfree(line);
        if (allow_incomplete) {
          i_tags_setn(&im->tags, "i_incomplete", 1);
          i_tags_setn(&im->tags, "i_lines_read", y);
          return im;
        }
        else {
          if (cp)
            i_push_error(0, "invalid data for ascii pnm");
          else
            i_push_error(0, "short read - file truncated?");
          i_img_destroy(im);
          return NULL;
        }
      }
      *linep++ = *cp == '0' ? 0 : 1;
    }
    i_ppal(im, 0, width, y, line);
  }
  myfree(line);

  return im;
}

static
i_img *
read_pgm_ppm_ascii(mbuf *mb, i_img *im, int width, int height, int channels, 
                   int maxval, int allow_incomplete) {
  i_color *line, *linep;
  int x, y, ch;
  int rounder = maxval / 2;

  line = mymalloc(width * sizeof(i_color));
  for(y=0;y<height;y++) {
    linep = line;
    for(x=0; x<width; x++) {
      for(ch=0; ch<channels; ch++) {
        int sample;
        
        if (!gnum(mb, &sample)) {
          myfree(line);
          if (allow_incomplete) {
            i_tags_setn(&im->tags, "i_incomplete", 1);
            i_tags_setn(&im->tags, "i_lines_read", 1);
            return im;
          }
          else {
            if (gpeek(mb))
              i_push_error(0, "invalid data for ascii pnm");
            else
              i_push_error(0, "short read - file truncated?");
            i_img_destroy(im);
            return NULL;
          }
        }
        if (sample > maxval)
          sample = maxval;
        linep->channel[ch] = (sample * 255 + rounder) / maxval;
      }
      ++linep;
    }
    i_plin(im, 0, width, y, line);
  }
  myfree(line);

  return im;
}

static
i_img *
read_pgm_ppm_ascii_16(mbuf *mb, i_img *im, int width, int height, 
                      int channels, int maxval, int allow_incomplete) {
  i_fcolor *line, *linep;
  int x, y, ch;
  double maxvalf = maxval;

  line = mymalloc(width * sizeof(i_fcolor));
  for(y=0;y<height;y++) {
    linep = line;
    for(x=0; x<width; x++) {
      for(ch=0; ch<channels; ch++) {
        int sample;
        
        if (!gnum(mb, &sample)) {
          myfree(line);
          if (allow_incomplete) {
	    i_tags_setn(&im->tags, "i_incomplete", 1);
	    i_tags_setn(&im->tags, "i_lines_read", y);
	    return im;
          }
          else {
            if (gpeek(mb))
              i_push_error(0, "invalid data for ascii pnm");
            else
              i_push_error(0, "short read - file truncated?");
            i_img_destroy(im);
            return NULL;
          }
        }
        if (sample > maxval)
          sample = maxval;
        linep->channel[ch] = sample / maxvalf;
      }
      ++linep;
    }
    i_plinf(im, 0, width, y, line);
  }
  myfree(line);

  return im;
}

/*
=item i_readpnm_wiol(ig, allow_incomplete)

Retrieve an image and stores in the iolayer object. Returns NULL on fatal error.

   ig     - io_glue object
   allow_incomplete - allows a partial file to be read successfully

=cut
*/
static i_img *i_readpnm_wiol_low( mbuf*, int);

i_img *
i_readpnm_wiol(io_glue *ig, int allow_incomplete) {
  mbuf buf;
  io_glue_commit_types(ig);
  init_buf(&buf, ig);

  return i_readpnm_wiol_low( &buf, allow_incomplete );
}

static i_img *
i_readpnm_wiol_low( mbuf *buf, int allow_incomplete) {
  i_img* im;
  int type;
  int width, height, maxval, channels, pcount;
  int rounder;
  char *cp;

  i_clear_error();
  mm_log((1,"i_readpnm(ig %p, allow_incomplete %d)\n", buf->ig, allow_incomplete));

  cp = gnext(buf);

  if (!cp || *cp != 'P') {
    i_push_error(0, "bad header magic, not a PNM file");
    mm_log((1, "i_readpnm: Could not read header of file\n"));
    return NULL;
  }

  if ( !(cp = gnext(buf)) ) {
    mm_log((1, "i_readpnm: Could not read header of file\n"));
    return NULL;
  }
  
  type = *cp-'0';

  if (type < 1 || type > 6) {
    i_push_error(0, "unknown PNM file type, not a PNM file");
    mm_log((1, "i_readpnm: Not a pnm file\n"));
    return NULL;
  }

  if ( !(cp = gnext(buf)) ) {
    mm_log((1, "i_readpnm: Could not read header of file\n"));
    return NULL;
  }
  
  if ( !misspace(*cp) ) {
    i_push_error(0, "unexpected character, not a PNM file");
    mm_log((1, "i_readpnm: Not a pnm file\n"));
    return NULL;
  }
  
  mm_log((1, "i_readpnm: image is a %s\n", typenames[type-1] ));

  
  /* Read sizes and such */

  if (!skip_comment(buf)) {
    i_push_error(0, "while skipping to width");
    mm_log((1, "i_readpnm: error reading before width\n"));
    return NULL;
  }
  
  if (!gnum(buf, &width)) {
    i_push_error(0, "could not read image width");
    mm_log((1, "i_readpnm: error reading width\n"));
    return NULL;
  }

  if (!skip_comment(buf)) {
    i_push_error(0, "while skipping to height");
    mm_log((1, "i_readpnm: error reading before height\n"));
    return NULL;
  }

  if (!gnum(buf, &height)) {
    i_push_error(0, "could not read image height");
    mm_log((1, "i_readpnm: error reading height\n"));
    return NULL;
  }
  
  if (!(type == 1 || type == 4)) {
    if (!skip_comment(buf)) {
      i_push_error(0, "while skipping to maxval");
      mm_log((1, "i_readpnm: error reading before maxval\n"));
      return NULL;
    }

    if (!gnum(buf, &maxval)) {
      i_push_error(0, "could not read maxval");
      mm_log((1, "i_readpnm: error reading maxval\n"));
      return NULL;
    }

    if (maxval == 0) {
      i_push_error(0, "maxval is zero - invalid pnm file");
      mm_log((1, "i_readpnm: maxval is zero, invalid pnm file\n"));
      return NULL;
    }
    else if (maxval > 65535) {
      i_push_errorf(0, "maxval of %d is over 65535 - invalid pnm file", 
		    maxval);
      mm_log((1, "i_readpnm: maxval of %d is over 65535 - invalid pnm file\n"));
      return NULL;
    }
  } else maxval=1;
  rounder = maxval / 2;

  if (!(cp = gnext(buf)) || !misspace(*cp)) {
    i_push_error(0, "garbage in header, invalid PNM file");
    mm_log((1, "i_readpnm: garbage in header\n"));
    return NULL;
  }

  channels = (type == 3 || type == 6) ? 3:1;
  pcount = width*height*channels;

  if (!i_int_check_image_file_limits(width, height, channels, sizeof(i_sample_t))) {
    mm_log((1, "i_readpnm: image size exceeds limits\n"));
    return NULL;
  }

  mm_log((1, "i_readpnm: (%d x %d), channels = %d, maxval = %d\n", width, height, channels, maxval));

  if (type == 1 || type == 4) {
    i_color pbm_pal[2];
    pbm_pal[0].channel[0] = 255;
    pbm_pal[1].channel[0] = 0;
    
    im = i_img_pal_new(width, height, 1, 256);
    i_addcolors(im, pbm_pal, 2);
  }
  else {
    if (maxval > 255)
      im = i_img_16_new(width, height, channels);
    else
      im = i_img_8_new(width, height, channels);
  }

  switch (type) {
  case 1: /* Ascii types */
    im = read_pbm_ascii(buf, im, width, height, allow_incomplete);
    break;

  case 2:
  case 3:
    if (maxval > 255)
      im = read_pgm_ppm_ascii_16(buf, im, width, height, channels, maxval, allow_incomplete);
    else
      im = read_pgm_ppm_ascii(buf, im, width, height, channels, maxval, allow_incomplete);
    break;
    
  case 4: /* binary pbm */
    im = read_pbm_bin(buf, im, width, height, allow_incomplete);
    break;

  case 5: /* binary pgm */
  case 6: /* binary ppm */
    if (maxval > 255)
      im = read_pgm_ppm_bin16(buf, im, width, height, channels, maxval, allow_incomplete);
    else
      im = read_pgm_ppm_bin8(buf, im, width, height, channels, maxval, allow_incomplete);
    break;

  default:
    mm_log((1, "type %s [P%d] unsupported\n", typenames[type-1], type));
    return NULL;
  }

  if (!im)
    return NULL;

  i_tags_add(&im->tags, "i_format", 0, "pnm", -1, 0);
  i_tags_setn(&im->tags, "pnm_maxval", maxval);
  i_tags_setn(&im->tags, "pnm_type", type);

  return im;
}

static void free_images(i_img **imgs, int count) {
  int i;

  if (count) {
    for (i = 0; i < count; ++i)
      i_img_destroy(imgs[i]);
    myfree(imgs);
  }
}

i_img **i_readpnm_multi_wiol(io_glue *ig, int *count, int allow_incomplete) {
    i_img **results = NULL;
    i_img *img = NULL;
    char *cp = NULL;
    mbuf buf;
    int result_alloc = 0, 
        value = 0, 
        eof = 0;
    *count=0;
    io_glue_commit_types(ig);
    init_buf(&buf, ig);
    do {
        mm_log((1, "read image %i\n", 1+*count));
        img = i_readpnm_wiol_low( &buf, allow_incomplete );
        if( !img ) {
            free_images( results, *count );
            return NULL;
        }
        ++*count;
        if (*count > result_alloc) {
            if (result_alloc == 0) {
                result_alloc = 5;
                results = mymalloc(result_alloc * sizeof(i_img *));
            }
            else {
                /* myrealloc never fails (it just dies if it can't allocate) */
                result_alloc *= 2;
                results = myrealloc(results, result_alloc * sizeof(i_img *));
            }
        }
        results[*count-1] = img;


        if( i_tags_get_int(&img->tags, "i_incomplete", 0, &value ) && value) {
            eof = 1;
        }
        else if( skip_spaces( &buf ) && ( cp=gpeek( &buf ) ) && *cp == 'P' ) {
            eof = 0;
        }
        else {
            eof = 1;
        }
    } while(!eof);
    return results;
}



static
int
write_pbm(i_img *im, io_glue *ig, int zero_is_white) {
  int x, y;
  i_palidx *line;
  int write_size;
  unsigned char *write_buf;
  unsigned char *writep;
  char header[255];
  unsigned mask;

  sprintf(header, "P4\012# CREATOR: Imager\012%d %d\012", 
          im->xsize, im->ysize);
  if (i_io_write(ig, header, strlen(header)) < 0) {
    i_push_error(0, "could not write pbm header");
    return 0;
  }
  write_size = (im->xsize + 7) / 8;
  line = mymalloc(sizeof(i_palidx) * im->xsize);
  write_buf = mymalloc(write_size);
  for (y = 0; y < im->ysize; ++y) {
    i_gpal(im, 0, im->xsize, y, line);
    mask = 0x80;
    writep = write_buf;
    memset(write_buf, 0, write_size);
    for (x = 0; x < im->xsize; ++x) {
      if (zero_is_white ? line[x] : !line[x])
        *writep |= mask;
      mask >>= 1;
      if (!mask) {
        ++writep;
        mask = 0x80;
      }
    }
    if (i_io_write(ig, write_buf, write_size) != write_size) {
      i_push_error(0, "write failure");
      myfree(write_buf);
      myfree(line);
      return 0;
    }
  }
  myfree(write_buf);
  myfree(line);

  return 1;
}

static
int
write_ppm_data_8(i_img *im, io_glue *ig, int want_channels) {
  int write_size = im->xsize * want_channels;
  int buf_size = im->xsize * im->channels;
  unsigned char *data = mymalloc(buf_size);
  int y = 0;
  int rc = 1;
  i_color bg;

  i_get_file_background(im, &bg);
  while (y < im->ysize && rc >= 0) {
    i_gsamp_bg(im, 0, im->xsize, y, data, want_channels, &bg);
    if (i_io_write(ig, data, write_size) != write_size) {
      i_push_error(errno, "could not write ppm data");
      rc = 0;
      break;
    }
    ++y;
  }
  myfree(data);

  return rc;
}

static
int
write_ppm_data_16(i_img *im, io_glue *ig, int want_channels) {
  int line_size = im->channels * im->xsize * sizeof(i_fsample_t);
  int sample_count = want_channels * im->xsize;
  int write_size = sample_count * 2;
  i_fsample_t *line_buf = mymalloc(line_size);
  i_fsample_t *samplep;
  unsigned char *write_buf = mymalloc(write_size);
  unsigned char *writep;
  int sample_num;
  int y = 0;
  int rc = 1;
  i_fcolor bg;

  i_get_file_backgroundf(im, &bg);

  while (y < im->ysize) {
    i_gsampf_bg(im, 0, im->xsize, y, line_buf, want_channels, &bg);
    samplep = line_buf;
    writep = write_buf;
    for (sample_num = 0; sample_num < sample_count; ++sample_num) {
      unsigned sample16 = SampleFTo16(*samplep++);
      *writep++ = sample16 >> 8;
      *writep++ = sample16 & 0xFF;
    }
    if (i_io_write(ig, write_buf, write_size) != write_size) {
      i_push_error(errno, "could not write ppm data");
      rc = 0;
      break;
    }
    ++y;
  }
  myfree(line_buf);
  myfree(write_buf);

  return rc;
}

undef_int
i_writeppm_wiol(i_img *im, io_glue *ig) {
  char header[255];
  int zero_is_white;
  int wide_data;

  mm_log((1,"i_writeppm(im %p, ig %p)\n", im, ig));
  i_clear_error();

  /* Add code to get the filename info from the iolayer */
  /* Also add code to check for mmapped code */

  io_glue_commit_types(ig);

  if (i_img_is_monochrome(im, &zero_is_white)) {
    return write_pbm(im, ig, zero_is_white);
  }
  else {
    int type;
    int maxval;
    int want_channels = im->channels;

    if (want_channels == 2 || want_channels == 4)
      --want_channels;

    if (!i_tags_get_int(&im->tags, "pnm_write_wide_data", 0, &wide_data))
      wide_data = 0;

    if (want_channels == 3) {
      type = 6;
    }
    else if (want_channels == 1) {
      type = 5;
    }
    else {
      i_push_error(0, "can only save 1 or 3 channel images to pnm");
      mm_log((1,"i_writeppm: ppm/pgm is 1 or 3 channel only (current image is %d)\n",im->channels));
      return(0);
    }
    if (im->bits <= 8 || !wide_data)
      maxval = 255;
    else
      maxval = 65535;

    sprintf(header,"P%d\n#CREATOR: Imager\n%d %d\n%d\n", 
            type, im->xsize, im->ysize, maxval);

    if (ig->writecb(ig,header,strlen(header)) != strlen(header)) {
      i_push_error(errno, "could not write ppm header");
      mm_log((1,"i_writeppm: unable to write ppm header.\n"));
      return(0);
    }

    if (!im->virtual && im->bits == i_8_bits && im->type == i_direct_type
	&& im->channels == want_channels) {
      if (ig->writecb(ig,im->idata,im->bytes) != im->bytes) {
        i_push_error(errno, "could not write ppm data");
        return 0;
      }
    }
    else if (maxval == 255) {
      if (!write_ppm_data_8(im, ig, want_channels))
        return 0;
    }
    else {
      if (!write_ppm_data_16(im, ig, want_channels))
        return 0;
    }
  }
  ig->closecb(ig);

  return(1);
}

/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>, Tony Cook <tonyc@cpan.org>,
Philip Gwyn <gwyn@cpan.org>.

=head1 SEE ALSO

Imager(3)

=cut
*/
