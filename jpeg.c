#include <stdio.h>
#include <sys/stat.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <setjmp.h>

#include "iolayer.h"
#include "image.h"
#include "jpeglib.h"


unsigned char fake_eoi[]={(JOCTET) 0xFF,(JOCTET) JPEG_EOI};

/* Handlers to read from memory */

typedef struct {
  struct jpeg_source_mgr pub;	/* public fields */
  char *data;
  int length;
  JOCTET * buffer;		/* start of buffer */
  boolean start_of_file;	/* have we gotten any data yet? */
} scalar_source_mgr;

typedef scalar_source_mgr *scalar_src_ptr;









static void
scalar_init_source (j_decompress_ptr cinfo) {
  scalar_src_ptr src = (scalar_src_ptr) cinfo->src;
  
  /* We reset the empty-input-file flag for each image,
   * but we don't clear the input buffer.
   * This is correct behavior for reading a series of images from one source.
   */
  src->start_of_file = TRUE;
}

static boolean
scalar_fill_input_buffer (j_decompress_ptr cinfo) {
  scalar_src_ptr src = (scalar_src_ptr) cinfo->src;
  size_t nbytes;
  
  if (src->start_of_file) {
    nbytes=src->length;
    src->buffer=src->data;
  } else {
    /* Insert a fake EOI marker */
    src->buffer = fake_eoi;
    nbytes = 2;
  }

  src->pub.next_input_byte = src->buffer;
  src->pub.bytes_in_buffer = nbytes;
  src->start_of_file = FALSE;
  return TRUE;
}

static void
scalar_skip_input_data (j_decompress_ptr cinfo, long num_bytes) {
  scalar_src_ptr src = (scalar_src_ptr) cinfo->src;
  
  /* Just a dumb implementation for now.  Could use fseek() except
   * it doesn't work on pipes.  Not clear that being smart is worth
   * any trouble anyway --- large skips are infrequent.
   */
  
  if (num_bytes > 0) {
    while (num_bytes > (long) src->pub.bytes_in_buffer) {
      num_bytes -= (long) src->pub.bytes_in_buffer;
      (void) scalar_fill_input_buffer(cinfo);
      /* note we assume that fill_input_buffer will never return FALSE,
       * so suspension need not be handled.
       */
    }
    src->pub.next_input_byte += (size_t) num_bytes;
    src->pub.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
scalar_term_source (j_decompress_ptr cinfo) {  /* no work necessary here */ }

static void
jpeg_scalar_src(j_decompress_ptr cinfo, char *data, int length) {
  scalar_src_ptr src;
  
  if (cinfo->src == NULL) {	/* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *) (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,sizeof(scalar_source_mgr));
    src = (scalar_src_ptr) cinfo->src;
  }
  
  src = (scalar_src_ptr) cinfo->src;
  src->data = data;
  src->length = length;
  src->pub.init_source = scalar_init_source;
  src->pub.fill_input_buffer = scalar_fill_input_buffer;
  src->pub.skip_input_data = scalar_skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->pub.term_source = scalar_term_source;
  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */
}




undef_int
i_writejpeg(i_img *im,int fd,int qfactor) {
  struct stat stbuf;
  JSAMPLE *image_buffer;
  int quality;

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;

  FILE *outfile;		/* target file */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */

  mm_log((1,"i_writejpeg(0x%x,fd %d,qfactor %d)\n",im,fd,qfactor));
  
  if (!(im->channels==1 || im->channels==3)) { fprintf(stderr,"Unable to write JPEG, improper colorspace.\n"); exit(3); }
  quality = qfactor;

  image_buffer=im->data;

  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  cinfo.err = jpeg_std_error(&jerr);
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  /* Here we use the library-supplied code to send compressed data to a
   * stdio stream.  You can also write your own code to do something else.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to write binary files.
   */
  
  if (fstat(fd,&stbuf)<0) { fprintf(stderr,"Unable to stat fd.\n"); exit(1); }
  
  if ((outfile = fdopen(fd,"w")) == NULL) {
    fprintf(stderr, "can't fdopen.\n");
    exit(1);
  }

  jpeg_stdio_dest(&cinfo, outfile);

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  cinfo.image_width = im->xsize; 	/* image width and height, in pixels */
  cinfo.image_height = im->ysize;

  if (im->channels==3) {
    cinfo.input_components = 3;		/* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
  }

  if (im->channels==1) {
    cinfo.input_components = 1;		/* # of color components per pixel */
    cinfo.in_color_space = JCS_GRAYSCALE; 	/* colorspace of input image */
  }

  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  jpeg_set_defaults(&cinfo);
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */

  jpeg_set_quality(&cinfo, quality, TRUE);  /* limit to baseline-JPEG values */

  /* Step 4: Start compressor */
  
  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
  row_stride = im->xsize * im->channels;	/* JSAMPLEs per row in image_buffer */

  while (cinfo.next_scanline < cinfo.image_height) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
    row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);
  /* After finish_compress, we can close the output file. */
  fclose(outfile);

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);

  return(1);
}


static int tlength=0;
static char **iptc_text=NULL;

#define JPEG_APP13       0xED    /* APP13 marker code */

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
my_output_message (j_common_ptr cinfo)
{
  char buffer[JMSG_LENGTH_MAX];

  /* Create the message */
  (*cinfo->err->format_message) (cinfo, buffer);

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






i_img*
i_readjpeg(int fd,char** iptc_itext,int *itlength) {
  i_img *im;

  struct jpeg_decompress_struct cinfo;
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */

  /*   struct jpeg_error_mgr jerr;*/
  struct my_error_mgr jerr;
  FILE * infile;		/* source file */
  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */

  mm_log((1,"i_readjpeg(fd %d,iptc_itext 0x%x)\n",fd,iptc_itext));

  iptc_text=iptc_itext;

  if ((infile = fdopen(fd,"r")) == NULL) {
    fprintf(stderr, "can't fdopen.\n");
    exit(1);
  }
  
  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  jerr.pub.output_message = my_output_message;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(&cinfo); 
    fclose(infile);
    *iptc_itext=NULL;
    *itlength=0;
    return NULL;
  }
  
  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);
  jpeg_set_marker_processor(&cinfo, JPEG_APP13, APP13_handler);
  /* Step 2: specify data source (eg, a file) */

  jpeg_stdio_src(&cinfo, infile);

  /* Step 3: read file parameters with jpeg_read_header() */

  (void) jpeg_read_header(&cinfo, TRUE);
  
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.doc for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */
  
  /* Step 5: Start decompressor */

  (void) jpeg_start_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */ 

  im=i_img_empty_ch(NULL,cinfo.output_width,cinfo.output_height,cinfo.output_components);

  /*  fprintf(stderr,"JPEG info:\n  xsize:%d\n  ysize:%d\n  channels:%d.\n",xsize,ysize,channels);*/ 

  /* JSAMPLEs per row in output buffer */
  row_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
  buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */

  while (cinfo.output_scanline < cinfo.output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
    /* Assume put_scanline_someplace wants a pointer and sample count. */
    memcpy(im->data+im->channels*im->xsize*(cinfo.output_scanline-1),buffer[0],row_stride);
  }

  /* Step 7: Finish decompression */

  (void) jpeg_finish_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress(&cinfo);

  /* After finish_decompress, we can close the input file.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */

/*  fclose(infile); DO NOT fclose() BECAUSE THEN close() WILL FAIL*/

  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
   */

  /* And we're done! */

  *itlength=tlength;
  mm_log((1,"i_readjpeg -> (0x%x)\n",im));
  return im;
}



i_img*
i_readjpeg_scalar(char *data, int length,char** iptc_itext,int *itlength) {
  i_img *im;

  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */

  mm_log((1,"i_readjpeg_scalar(data 0x%08x, length %d,iptc_itext 0x%x)\n",data,length,iptc_itext));
  iptc_text=iptc_itext;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  jerr.pub.output_message = my_output_message;
  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_destroy_decompress(&cinfo); 
    *iptc_itext=NULL;
    *itlength=0;
    return NULL;
  }
  
  jpeg_create_decompress(&cinfo);
  jpeg_set_marker_processor(&cinfo, JPEG_APP13, APP13_handler);
  jpeg_scalar_src(&cinfo, data, length );
  (void) jpeg_read_header(&cinfo, TRUE);
  (void) jpeg_start_decompress(&cinfo);
  im=i_img_empty_ch(NULL,cinfo.output_width,cinfo.output_height,cinfo.output_components);
  row_stride = cinfo.output_width * cinfo.output_components;
  buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
  while (cinfo.output_scanline < cinfo.output_height) {
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
    memcpy(im->data+im->channels*im->xsize*(cinfo.output_scanline-1),buffer[0],row_stride);
  }
  (void) jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  *itlength=tlength;
  mm_log((1,"i_readjpeg_scalar -> (0x%x)\n",im));
  return im;
}





















































#define JPGS 1024



typedef struct {
  struct jpeg_source_mgr pub;	/* public fields */
  io_glue *data;
  JOCTET *buffer;		/* start of buffer */
  int length;			/* Do I need this? */
  boolean start_of_file;	/* have we gotten any data yet? */
} wiol_source_mgr;

typedef wiol_source_mgr *wiol_src_ptr;

static void
wiol_init_source (j_decompress_ptr cinfo) {
  wiol_src_ptr src = (wiol_src_ptr) cinfo->src;
  
  /* We reset the empty-input-file flag for each image,
   * but we don't clear the input buffer.
   * This is correct behavior for reading a series of images from one source.
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












i_img*
i_readjpeg_wiol(io_glue *data, int length, char** iptc_itext, int *itlength) {
  i_img *im;

  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */

  mm_log((1,"i_readjpeg_wiol(data 0x%p, length %d,iptc_itext 0x%p)\n", data, iptc_itext));

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
  jpeg_wiol_src(&cinfo, data, length);

  (void) jpeg_read_header(&cinfo, TRUE);
  (void) jpeg_start_decompress(&cinfo);
  im=i_img_empty_ch(NULL,cinfo.output_width,cinfo.output_height,cinfo.output_components);
  row_stride = cinfo.output_width * cinfo.output_components;
  buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
  while (cinfo.output_scanline < cinfo.output_height) {
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
    memcpy(im->data+im->channels*im->xsize*(cinfo.output_scanline-1),buffer[0],row_stride);
  }
  (void) jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  *itlength=tlength;
  mm_log((1,"i_readjpeg_wiol -> (0x%x)\n",im));
  return im;
}


