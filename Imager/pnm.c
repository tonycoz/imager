#include "image.h"
#include "log.h"
#include "iolayer.h"

#include <stdlib.h>
#include <errno.h>


/*
=head1 NAME

pnm.c - implements reading and writing ppm/pnm/pbm files, uses io layer.

=head1 SYNOPSIS

   io_glue *ig = io_new_fd( fd );
   i_img *im   = i_readpnm_wiol(ig, -1); // no limit on how much is read
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

static
char *
gnext(mbuf *mb) {
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
      i_push_error(errno, "unexpected end of file");
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

static
char *
gpeek(mbuf *mb) {
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
      i_push_error(0, "unexpected end of file");
      mm_log((1, "i_readpnm: end of file\n"));
      return NULL;
    }
  }
  return &mb->buf[mb->cp];
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

  while( (cp = gpeek(mb)) && misnumber(*cp) ) {
    *i = *i*10+(*cp-'0');
    cp = gnext(mb);
  }
  return 1;
}


/*
=item i_readpnm_wiol(ig, length)

Retrieve an image and stores in the iolayer object. Returns NULL on fatal error.

   ig     - io_glue object
   length - maximum length to read from data source, before closing it -1 
            signifies no limit.

=cut
*/


i_img *
i_readpnm_wiol(io_glue *ig, int length) {
  i_img* im;
  int type;
  int x, y, ch;
  int width, height, maxval, channels, pcount;
  int rounder;
  char *cp;
  unsigned char *uc;
  mbuf buf;
  i_color val;

  i_clear_error();

  mm_log((1,"i_readpnm(ig %p, length %d)\n", ig, length));

  io_glue_commit_types(ig);
  init_buf(&buf, ig);

  cp = gnext(&buf);

  if (!cp || *cp != 'P') {
    i_push_error(0, "bad header magic, not a PNM file");
    mm_log((1, "i_readpnm: Could not read header of file\n"));
    return NULL;
  }

  if ( !(cp = gnext(&buf)) ) {
    mm_log((1, "i_readpnm: Could not read header of file\n"));
    return NULL;
  }
  
  type = *cp-'0';

  if (type < 1 || type > 6) {
    i_push_error(0, "unknown PNM file type, not a PNM file");
    mm_log((1, "i_readpnm: Not a pnm file\n"));
    return NULL;
  }

  if ( !(cp = gnext(&buf)) ) {
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

  if (!skip_comment(&buf)) {
    i_push_error(0, "while skipping to width");
    mm_log((1, "i_readpnm: error reading before width\n"));
    return NULL;
  }
  
  if (!gnum(&buf, &width)) {
    i_push_error(0, "could not read image width");
    mm_log((1, "i_readpnm: error reading width\n"));
    return NULL;
  }

  if (!skip_comment(&buf)) {
    i_push_error(0, "while skipping to height");
    mm_log((1, "i_readpnm: error reading before height\n"));
    return NULL;
  }

  if (!gnum(&buf, &height)) {
    i_push_error(0, "could not read image height");
    mm_log((1, "i_readpnm: error reading height\n"));
    return NULL;
  }
  
  if (!(type == 1 || type == 4)) {
    if (!skip_comment(&buf)) {
      i_push_error(0, "while skipping to maxval");
      mm_log((1, "i_readpnm: error reading before maxval\n"));
      return NULL;
    }

    if (!gnum(&buf, &maxval)) {
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
    else if (type >= 4 && maxval > 255) {
      i_push_errorf(0, "maxval of %d is over 255 - not currently supported by Imager for binary pnm", maxval);
      mm_log((1, "i_readpnm: maxval of %d is over 255 - not currently supported by Imager for binary pnm\n", maxval));
      return NULL;
    }
  } else maxval=1;
  rounder = maxval / 2;

  if (!(cp = gnext(&buf)) || !misspace(*cp)) {
    i_push_error(0, "garbage in header, invalid PNM file");
    mm_log((1, "i_readpnm: garbage in header\n"));
    return NULL;
  }

  channels = (type == 3 || type == 6) ? 3:1;
  pcount = width*height*channels;

  mm_log((1, "i_readpnm: (%d x %d), channels = %d, maxval = %d\n", width, height, channels, maxval));
  
  im = i_img_empty_ch(NULL, width, height, channels);

  i_tags_add(&im->tags, "i_format", 0, "pnm", -1, 0);

  switch (type) {
  case 1: /* Ascii types */
  case 2:
  case 3:
    for(y=0;y<height;y++) for(x=0; x<width; x++) {
      for(ch=0; ch<channels; ch++) {
	int t;
	if (gnum(&buf, &t)) val.channel[ch] = (t * 255 + rounder) / maxval;
	else {
	  mm_log((1,"i_readpnm: gnum() returned false in data\n"));
	  return im;
	}
      }
      i_ppix(im, x, y, &val);
    }
    break;
    
  case 4: /* binary pbm */
    for(y=0;y<height;y++) for(x=0; x<width; x+=8) {
      if ( (uc = (unsigned char*)gnext(&buf)) ) {
	int xt;
	int pc = width-x < 8 ? width-x : 8;
	/*	mm_log((1,"i_readpnm: y=%d x=%d pc=%d\n", y, x, pc)); */
	for(xt = 0; xt<pc; xt++) {
	  val.channel[0] = (*uc & (128>>xt)) ? 0 : 255; 
	  i_ppix(im, x+xt, y, &val);
	}
      } else {
	mm_log((1,"i_readpnm: gnext() returned false in data\n"));
	return im;
      }
    }
    break;

  case 5: /* binary pgm */
  case 6: /* binary ppm */
    for(y=0;y<height;y++) for(x=0; x<width; x++) {
      for(ch=0; ch<channels; ch++) {
	if ( (uc = (unsigned char*)gnext(&buf)) ) 
	  val.channel[ch] = (*uc * 255 + rounder) / maxval;
	else {
	  mm_log((1,"i_readpnm: gnext() returned false in data\n"));
	  return im;
	}
      }
      i_ppix(im, x, y, &val);
    }
    break;
  default:
    mm_log((1, "type %s [P%d] unsupported\n", typenames[type-1], type));
    return NULL;
  }
  return im;
}


undef_int
i_writeppm_wiol(i_img *im, io_glue *ig) {
  char header[255];
  int rc;

  mm_log((1,"i_writeppm(im %p, ig %p)\n", im, ig));
  i_clear_error();

  /* Add code to get the filename info from the iolayer */
  /* Also add code to check for mmapped code */

  io_glue_commit_types(ig);

  if (im->channels == 3) {
    sprintf(header,"P6\n#CREATOR: Imager\n%d %d\n255\n",im->xsize,im->ysize);
    if (ig->writecb(ig,header,strlen(header))<0) {
      i_push_error(errno, "could not write ppm header");
      mm_log((1,"i_writeppm: unable to write ppm header.\n"));
      return(0);
    }

    if (!im->virtual && im->bits == i_8_bits && im->type == i_direct_type) {
      rc = ig->writecb(ig,im->idata,im->bytes);
    }
    else {
      unsigned char *data = mymalloc(3 * im->xsize);
      if (data != NULL) {
        int y = 0;
        static int rgb_chan[3] = { 0, 1, 2 };

        rc = 0;
        while (y < im->ysize && rc >= 0) {
          i_gsamp(im, 0, im->xsize, y, data, rgb_chan, 3);
          rc = ig->writecb(ig, data, im->xsize * 3);
          ++y;
        }
        myfree(data);
      }
      else {
        i_push_error(0, "Out of memory");
        return 0;
      }
    }
    if (rc<0) {
      i_push_error(errno, "could not write ppm data");
      mm_log((1,"i_writeppm: unable to write ppm data.\n"));
      return(0);
    }
  }
  else if (im->channels == 1) {
    sprintf(header, "P5\n#CREATOR: Imager\n%d %d\n255\n",
	    im->xsize, im->ysize);
    if (ig->writecb(ig,header, strlen(header)) < 0) {
      i_push_error(errno, "could not write pgm header");
      mm_log((1,"i_writeppm: unable to write pgm header.\n"));
      return(0);
    }

    if (!im->virtual && im->bits == i_8_bits && im->type == i_direct_type) {
      rc=ig->writecb(ig,im->idata,im->bytes);
    }
    else {
      unsigned char *data = mymalloc(im->xsize);
      if (data != NULL) {
        int y = 0;
        int chan = 0;

        rc = 0;
        while (y < im->ysize && rc >= 0) {
          i_gsamp(im, 0, im->xsize, y, data, &chan, 1);
          rc = ig->writecb(ig, data, im->xsize);
          ++y;
        }
        myfree(data);
      }
      else {
        i_push_error(0, "Out of memory");
        return 0;
      }
    }
    if (rc<0) {
      i_push_error(errno, "could not write pgm data");
      mm_log((1,"i_writeppm: unable to write pgm data.\n"));
      return(0);
    }
  }
  else {
    i_push_error(0, "can only save 1 or 3 channel images to pnm");
    mm_log((1,"i_writeppm: ppm/pgm is 1 or 3 channel only (current image is %d)\n",im->channels));
    return(0);
  }
  ig->closecb(ig);

  return(1);
}

/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson <addi@umich.edu>

=head1 SEE ALSO

Imager(3)

=cut
*/
