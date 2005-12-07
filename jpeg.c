/*
=head1 NAME

jpeg.c - implement saving and loading JPEG images

=head1 SYNOPSIS

  io_glue *ig;
  if (!i_writejpeg_wiol(im, ig, quality)) {
    .. error ..
  }
  im = i_readjpeg_wiol(ig, length, iptc_text, itlength);

=head1 DESCRIPTION

Reads and writes JPEG images

=over

=cut
*/

#include <stdio.h>
#include <sys/stat.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <setjmp.h>

#include "iolayer.h"
#include "imagei.h"
#include "jpeglib.h"
#include "jerror.h"
#include <errno.h>
#ifdef IMEXIF_ENABLE
#include "imexif.h"
#endif

#define JPEG_APP13       0xED    /* APP13 marker code */
#define JPEG_APP1 (JPEG_APP0 + 1)
#define JPGS 16384

static unsigned char fake_eoi[]={(JOCTET) 0xFF,(JOCTET) JPEG_EOI};

/* Bad design right here */

static int tlength=0;
static char **iptc_text=NULL;


/* Source and Destination managers */


typedef struct {
  struct jpeg_source_mgr pub;	/* public fields */
  io_glue *data;
  JOCTET *buffer;		/* start of buffer */
  int length;			/* Do I need this? */
  boolean start_of_file;	/* have we gotten any data yet? */
} wiol_source_mgr;

typedef struct {
  struct jpeg_destination_mgr pub; /* public fields */
  io_glue *data;
  JOCTET *buffer;		/* start of buffer */
  boolean start_of_file;	/* have we gotten any data yet? */
} wiol_destination_mgr;

typedef wiol_source_mgr *wiol_src_ptr;
typedef wiol_destination_mgr *wiol_dest_ptr;


/*
 * Methods for io manager objects 
 * 
 * Methods for source managers: 
 *
 * init_source         (j_decompress_ptr cinfo);
 * skip_input_data     (j_decompress_ptr cinfo, long num_bytes);
 * fill_input_buffer   (j_decompress_ptr cinfo);
 * term_source         (j_decompress_ptr cinfo);
 */



static void
wiol_init_source (j_decompress_ptr cinfo) {
  wiol_src_ptr src = (wiol_src_ptr) cinfo->src;
  
  /* We reset the empty-input-file flag for each image, but we don't clear
   * the input buffer.   This is correct behavior for reading a series of
   * images from one source.
   */
  src->start_of_file = TRUE;
}



static boolean
wiol_fill_input_buffer(j_decompress_ptr cinfo) {
  wiol_src_ptr src = (wiol_src_ptr) cinfo->src;
  ssize_t nbytes; /* We assume that reads are "small" */
  
  mm_log((1,"wiol_fill_input_buffer(cinfo 0x%p)\n"));
  
  nbytes = src->data->readcb(src->data, src->buffer, JPGS);
  
  if (nbytes <= 0) { /* Insert a fake EOI marker */
    src->pub.next_input_byte = fake_eoi;
    src->pub.bytes_in_buffer = 2;
  } else {
    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = nbytes;
  }
  src->start_of_file = FALSE;
  return TRUE;
}


static void
wiol_skip_input_data (j_decompress_ptr cinfo, long num_bytes) {
  wiol_src_ptr src = (wiol_src_ptr) cinfo->src;
  
  /* Just a dumb implementation for now.  Could use fseek() except
   * it doesn't work on pipes.  Not clear that being smart is worth
   * any trouble anyway --- large skips are infrequent.
   */
  
  if (num_bytes > 0) {
    while (num_bytes > (long) src->pub.bytes_in_buffer) {
      num_bytes -= (long) src->pub.bytes_in_buffer;
      (void) wiol_fill_input_buffer(cinfo);
      /* note we assume that fill_input_buffer will never return FALSE,
       * so suspension need not be handled.
       */
    }
    src->pub.next_input_byte += (size_t) num_bytes;
    src->pub.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
wiol_term_source (j_decompress_ptr cinfo) {
  /* no work necessary here */ 
  wiol_src_ptr src;
  if (cinfo->src != NULL) {
    src = (wiol_src_ptr) cinfo->src;
    myfree(src->buffer);
  }
}


/* Source manager Constructor */

static void
jpeg_wiol_src(j_decompress_ptr cinfo, io_glue *ig, int length) {
  wiol_src_ptr src;
  
  if (cinfo->src == NULL) {	/* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *) (*cinfo->mem->alloc_small) 
      ((j_common_ptr) cinfo, JPOOL_PERMANENT,sizeof(wiol_source_mgr));
    src = (wiol_src_ptr) cinfo->src;
  }

  /* put the request method call in here later */
  io_glue_commit_types(ig);
  
  src         = (wiol_src_ptr) cinfo->src;
  src->data   = ig;
  src->buffer = mymalloc( JPGS );
  src->length = length;

  src->pub.init_source       = wiol_init_source;
  src->pub.fill_input_buffer = wiol_fill_input_buffer;
  src->pub.skip_input_data   = wiol_skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->pub.term_source       = wiol_term_source;
  src->pub.bytes_in_buffer   = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte   = NULL; /* until buffer loaded */
}




/*
 * Methods for destination managers:
 *
 * init_destination    (j_compress_ptr cinfo);
 * empty_output_buffer (j_compress_ptr cinfo);
 * term_destination    (j_compress_ptr cinfo);
 *
 */

static void
wiol_init_destination (j_compress_ptr cinfo) {
  wiol_dest_ptr dest = (wiol_dest_ptr) cinfo->dest;
  
  /* We reset the empty-input-file flag for each image, but we don't clear
   * the input buffer.   This is correct behavior for reading a series of
   * images from one source.
   */
  dest->start_of_file = TRUE;
}

static boolean
wiol_empty_output_buffer(j_compress_ptr cinfo) {
  wiol_dest_ptr dest = (wiol_dest_ptr) cinfo->dest;
  ssize_t rc;
  /*
    Previously this code was checking free_in_buffer to see how much 
    needed to be written.  This does not follow the documentation:

                       "In typical applications, it should write out the
        *entire* buffer (use the saved start address and buffer length;
        ignore the current state of next_output_byte and free_in_buffer)."

  ssize_t nbytes     = JPGS - dest->pub.free_in_buffer;
  */

  mm_log((1,"wiol_empty_output_buffer(cinfo 0x%p)\n"));
  rc = dest->data->writecb(dest->data, dest->buffer, JPGS);

  if (rc != JPGS) { /* XXX: Should raise some jpeg error */
    myfree(dest->buffer);
    mm_log((1, "wiol_empty_output_buffer: Error: nbytes = %d != rc = %d\n", JPGS, rc));
    ERREXIT(cinfo, JERR_FILE_WRITE);
  }
  dest->pub.free_in_buffer = JPGS;
  dest->pub.next_output_byte = dest->buffer;
  return TRUE;
}

static void
wiol_term_destination (j_compress_ptr cinfo) {
  wiol_dest_ptr dest = (wiol_dest_ptr) cinfo->dest;
  size_t nbytes = JPGS - dest->pub.free_in_buffer;
  /* yes, this needs to flush the buffer */
  /* needs error handling */

  if (dest->data->writecb(dest->data, dest->buffer, nbytes) != nbytes) {
    myfree(dest->buffer);
    ERREXIT(cinfo, JERR_FILE_WRITE);
  }
  
  if (dest != NULL) myfree(dest->buffer);
}


/* Destination manager Constructor */

static void
jpeg_wiol_dest(j_compress_ptr cinfo, io_glue *ig) {
  wiol_dest_ptr dest;
  
  if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
    cinfo->dest = 
      (struct jpeg_destination_mgr *)
      (*cinfo->mem->alloc_small) 
      ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(wiol_destination_mgr));
  }
  
  dest = (wiol_dest_ptr) cinfo->dest;
  dest->data                    = ig;
  dest->buffer                  = mymalloc( JPGS );

  dest->pub.init_destination    = wiol_init_destination;
  dest->pub.empty_output_buffer = wiol_empty_output_buffer;
  dest->pub.term_destination    = wiol_term_destination;
  dest->pub.free_in_buffer      = JPGS;
  dest->pub.next_output_byte    = dest->buffer;
}

LOCAL(unsigned int)
jpeg_getc (j_decompress_ptr cinfo)
/* Read next byte */
{
  struct jpeg_source_mgr * datasrc = cinfo->src;

  if (datasrc->bytes_in_buffer == 0) {
    if (! (*datasrc->fill_input_buffer) (cinfo))
      { fprintf(stderr,"Jpeglib: cant suspend.\n"); exit(3); }
      /*      ERREXIT(cinfo, JERR_CANT_SUSPEND);*/
  }
  datasrc->bytes_in_buffer--;
  return GETJOCTET(*datasrc->next_input_byte++);
}

METHODDEF(boolean)
APP13_handler (j_decompress_ptr cinfo) {
  INT32 length;
  unsigned int cnt=0;
  
  length = jpeg_getc(cinfo) << 8;
  length += jpeg_getc(cinfo);
  length -= 2;	/* discount the length word itself */
  
  tlength=length;

  if ( ((*iptc_text)=mymalloc(length)) == NULL ) return FALSE;
  while (--length >= 0) (*iptc_text)[cnt++] = jpeg_getc(cinfo); 
 
  return TRUE;
}

METHODDEF(void)
my_output_message (j_common_ptr cinfo) {
  char buffer[JMSG_LENGTH_MAX];

  /* Create the message */
  (*cinfo->err->format_message) (cinfo, buffer);

  i_push_error(0, buffer);

  /* Send it to stderr, adding a newline */
  mm_log((1, "%s\n", buffer));
}

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */
  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/* Here's the routine that will replace the standard error_exit method */

METHODDEF(void)
my_error_exit (j_common_ptr cinfo) {
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;
  
  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

/*
=item i_readjpeg_wiol(data, length, iptc_itext, itlength)

=cut
*/
i_img*
i_readjpeg_wiol(io_glue *data, int length, char** iptc_itext, int *itlength) {
  i_img *im;
#ifdef IMEXIF_ENABLE
  int seen_exif = 0;
#endif

  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */
  jpeg_saved_marker_ptr markerp;

  mm_log((1,"i_readjpeg_wiol(data 0x%p, length %d,iptc_itext 0x%p)\n", data, iptc_itext));

  i_clear_error();

  iptc_text = iptc_itext;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit     = my_error_exit;
  jerr.pub.output_message = my_output_message;

  /* Set error handler */
  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_destroy_decompress(&cinfo); 
    *iptc_itext=NULL;
    *itlength=0;
    return NULL;
  }
  
  jpeg_create_decompress(&cinfo);
  jpeg_set_marker_processor(&cinfo, JPEG_APP13, APP13_handler);
  jpeg_save_markers(&cinfo, JPEG_APP1, 0xFFFF);
  jpeg_save_markers(&cinfo, JPEG_COM, 0xFFFF);
  jpeg_wiol_src(&cinfo, data, length);

  (void) jpeg_read_header(&cinfo, TRUE);
  (void) jpeg_start_decompress(&cinfo);
  if (!i_int_check_image_file_limits(cinfo.output_width, cinfo.output_height,
				     cinfo.output_components, sizeof(i_sample_t))) {
    mm_log((1, "i_readjpeg: image size exceeds limits\n"));

    jpeg_destroy_decompress(&cinfo);
    return NULL;
  }
  im=i_img_empty_ch(NULL,cinfo.output_width,cinfo.output_height,cinfo.output_components);
  if (!im) {
    jpeg_destroy_decompress(&cinfo);
    return NULL;
  }
  row_stride = cinfo.output_width * cinfo.output_components;
  buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
  while (cinfo.output_scanline < cinfo.output_height) {
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
    memcpy(im->idata+im->channels*im->xsize*(cinfo.output_scanline-1),buffer[0],row_stride);
  }

  /* check for APP1 marker and save */
  markerp = cinfo.marker_list;
  while (markerp != NULL) {
    if (markerp->marker == JPEG_COM) {
      i_tags_add(&im->tags, "jpeg_comment", 0, markerp->data,
		 markerp->data_length, 0);
    }
#ifdef IMEXIF_ENABLE
    else if (markerp->marker == JPEG_APP1 && !seen_exif) {
      seen_exif = i_int_decode_exif(im, markerp->data, markerp->data_length);
    }
#endif      

    markerp = markerp->next;
  }

  if (cinfo.saw_JFIF_marker) {
    double xres = cinfo.X_density;
    double yres = cinfo.Y_density;
    
    i_tags_addn(&im->tags, "jpeg_density_unit", 0, cinfo.density_unit);
    switch (cinfo.density_unit) {
    case 0: /* values are just the aspect ratio */
      i_tags_addn(&im->tags, "i_aspect_only", 0, 1);
      i_tags_add(&im->tags, "jpeg_density_unit_name", 0, "none", -1, 0);
      break;

    case 1: /* per inch */
      i_tags_add(&im->tags, "jpeg_density_unit_name", 0, "inch", -1, 0);
      break;

    case 2: /* per cm */
      i_tags_add(&im->tags, "jpeg_density_unit_name", 0, "centimeter", -1, 0);
      xres *= 2.54;
      yres *= 2.54;
      break;
    }
    i_tags_set_float2(&im->tags, "i_xres", 0, xres, 6);
    i_tags_set_float2(&im->tags, "i_yres", 0, yres, 6);
  }

  (void) jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  *itlength=tlength;

  i_tags_add(&im->tags, "i_format", 0, "jpeg", 4, 0);

  mm_log((1,"i_readjpeg_wiol -> (0x%x)\n",im));
  return im;
}

/*
=item i_writejpeg_wiol(im, ig, qfactor)

=cut
*/

undef_int
i_writejpeg_wiol(i_img *im, io_glue *ig, int qfactor) {
  JSAMPLE *image_buffer;
  int quality;
  int got_xres, got_yres, aspect_only, resunit;
  double xres, yres;
  int comment_entry;

  struct jpeg_compress_struct cinfo;
  struct my_error_mgr jerr;

  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */
  unsigned char * data = NULL;

  mm_log((1,"i_writejpeg(im %p, ig %p, qfactor %d)\n", im, ig, qfactor));
  
  i_clear_error();

  if (!(im->channels==1 || im->channels==3)) { 
    i_push_error(0, "only 1 or 3 channels images can be saved as JPEG");
    return 0;
  }
  quality = qfactor;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  jerr.pub.output_message = my_output_message;
  
  jpeg_create_compress(&cinfo);

  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_destroy_compress(&cinfo);
    if (data)
      myfree(data);
    return 0;
  }

  io_glue_commit_types(ig);
  jpeg_wiol_dest(&cinfo, ig);

  cinfo.image_width  = im -> xsize; 	/* image width and height, in pixels */
  cinfo.image_height = im -> ysize;

  if (im->channels==3) {
    cinfo.input_components = 3;		/* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
  }

  if (im->channels==1) {
    cinfo.input_components = 1;		/* # of color components per pixel */
    cinfo.in_color_space = JCS_GRAYSCALE; 	/* colorspace of input image */
  }

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, TRUE);  /* limit to baseline-JPEG values */

  got_xres = i_tags_get_float(&im->tags, "i_xres", 0, &xres);
  got_yres = i_tags_get_float(&im->tags, "i_yres", 0, &yres);
  if (!i_tags_get_int(&im->tags, "i_aspect_only", 0,&aspect_only))
    aspect_only = 0;
  if (!i_tags_get_int(&im->tags, "jpeg_density_unit", 0, &resunit))
    resunit = 1; /* per inch */
  if (resunit < 0 || resunit > 2) /* default to inch if invalid */
    resunit = 1;
  if (got_xres || got_yres) {
    if (!got_xres)
      xres = yres;
    else if (!got_yres)
      yres = xres;
    if (aspect_only)
      resunit = 0; /* standard tags override format tags */
    if (resunit == 2) {
      /* convert to per cm */
      xres /= 2.54;
      yres /= 2.54;
    }
    cinfo.density_unit = resunit;
    cinfo.X_density = (int)(xres + 0.5);
    cinfo.Y_density = (int)(yres + 0.5);
  }

  jpeg_start_compress(&cinfo, TRUE);

  if (i_tags_find(&im->tags, "jpeg_comment", 0, &comment_entry)) {
    jpeg_write_marker(&cinfo, JPEG_COM, im->tags.tags[comment_entry].data,
		      im->tags.tags[comment_entry].size);
  }

  row_stride = im->xsize * im->channels;	/* JSAMPLEs per row in image_buffer */

  if (!im->virtual && im->type == i_direct_type && im->bits == i_8_bits) {
    image_buffer=im->idata;

    while (cinfo.next_scanline < cinfo.image_height) {
      /* jpeg_write_scanlines expects an array of pointers to scanlines.
       * Here the array is only one element long, but you could pass
       * more than one scanline at a time if that's more convenient.
       */
      row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
      (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
  }
  else {
    data = mymalloc(im->xsize * im->channels);
    if (data) {
      while (cinfo.next_scanline < cinfo.image_height) {
        /* jpeg_write_scanlines expects an array of pointers to scanlines.
         * Here the array is only one element long, but you could pass
         * more than one scanline at a time if that's more convenient.
         */
        i_gsamp(im, 0, im->xsize, cinfo.next_scanline, data, 
                NULL, im->channels);
        row_pointer[0] = data;
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
      }
    }
    else {
      jpeg_destroy_compress(&cinfo);
      i_push_error(0, "out of memory");
      return 0; /* out of memory? */
    }
  }

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);

  jpeg_destroy_compress(&cinfo);

  ig->closecb(ig);

  return(1);
}

/*
=back

=head1 AUTHOR

Arnar M. Hrafnkelsson, addi@umich.edu

=head1 SEE ALSO

Imager(3)

=cut
*/
