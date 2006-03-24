#include "msicon.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

static int 
read_palette(ico_reader_t *file, ico_image_t *image, int *error);
static int 
read_32bit_data(ico_reader_t *file, ico_image_t *image, int *error);
static int 
read_8bit_data(ico_reader_t *file, ico_image_t *image, int *error);
static int 
read_4bit_data(ico_reader_t *file, ico_image_t *image, int *error);
static int 
read_1bit_data(ico_reader_t *file, ico_image_t *image, int *error);
static int 
read_mask(ico_reader_t *file, ico_image_t *image, int *error);

typedef struct {
  int width;
  int height;
  long offset;
  long size;
} ico_reader_image_entry;

/* this was previously declared, now define it */
struct ico_reader_tag {
  /* the file we're dealing with */
  i_io_glue_t *ig;

  /* number of images in the file */
  int count;

  /* type of resource - 1=icon, 2=cursor */
  int type;

  /* image information from the header */
  ico_reader_image_entry *images;
};

static
int read_packed(io_glue *ig, const char *format, ...);

/*
=head1 NAME 

msicon.c - functions for working with .ICO files.

=head1 SYNOPSIS

  // reading
  int error;
  ico_reader_t *file = ico_reader_open(ig, &error);
  if (!file) {
    char buffer[100];
    ico_error_message(error, buffer, sizeof(buffer));
    fputs(buffer, stderr);
    exit(1);
  }
  int count = ico_image_count(file);
  for (i = 0; i < count; ++i) {
    ico_image_t *im = ico_image_read(file, index);
    printf("%d x %d image %d\n", im->width, im->height, 
           im->direct ? "direct" : "paletted");
    ico_image_release(im);
  }
  ico_reader_close(file);

=head1 DESCRIPTION

This is intended as a general interface to reading MS Icon files, and
is written to be independent of Imager, even though it is part of
Imager.  You just need to supply something that acts like Imager's
io_glue.

It relies on icon images being generally small, and reads the entire
image into memory when reading.

=head1 READING ICON FILES

=over

=item ico_reader_open(ig, &error)

Parameters:

=over

=item *

io_glue *ig - an Imager IO object.  This must be seekable.

=item *

int *error - pointer to an integer which an error code will be
returned in on failure.

=back

=cut
*/

ico_reader_t *
ico_reader_open(i_io_glue_t *ig, int *error) {
  long res1, type, count;
  ico_reader_t *file = NULL;
  int i;

  if (!read_packed(ig, "www", &res1, &type, &count)) {
    *error = ICOERR_Short_File;
    return NULL;
  }
  if (res1 != 0 || (type != 1 && type != 2) || count == 0) {
    *error = ICOERR_Invalid_File;
    return NULL;
  }

  file = malloc(sizeof(ico_reader_t));
  if (!file) {
    *error = ICOERR_Out_Of_Memory;
    return NULL;
  }
  file->count = count;
  file->type = type;
  file->ig = ig;
  file->images = malloc(sizeof(ico_reader_image_entry) * count);
  if (file->images == NULL) {
    *error = ICOERR_Out_Of_Memory;
    free(file);
    return NULL;
  }

  for (i = 0; i < count; ++i) {
    long width, height, bytes_in_res, image_offset;
    ico_reader_image_entry *image = file->images + i;
    if (!read_packed(ig, "bbxxxxxxdd", &width, &height, &bytes_in_res, 
		     &image_offset)) {
      free(file->images);
      free(file);
      *error = ICOERR_Short_File;
      return NULL;
    }

    image->width = width;
    image->height = height;
    image->offset = image_offset;
    image->size = bytes_in_res;
  }

  return file;
}

/*
=item ico_image_count

  // number of images in the file
  count = ico_image_count(file);

=cut
*/

int
ico_image_count(ico_reader_t *file) {
  return file->count;
}

/*
=item ico_type

  // type of file - 1 for icon, 2 for cursor
  type = ico_type(file);

=cut
*/

int
ico_type(ico_reader_t *file) {
  return file->type;
}

/*
=item ico_image_read

Read an image from the file given it's index.

=cut
*/

ico_image_t *
ico_image_read(ico_reader_t *file, int index, int *error) {
  io_glue *ig = file->ig;
  ico_reader_image_entry *im;
  long bi_size, width, height, planes, bit_count;
  ico_image_t *result;

  if (index < 0 || index >= file->count) {
    *error = ICOERR_Bad_Image_Index;
    return NULL;
  }

  im = file->images + index;
  if (i_io_seek(ig, im->offset, SEEK_SET) != im->offset) {
    *error = ICOERR_File_Error;
    return NULL;
  }

  if (!read_packed(ig, "dddww xxxx xxxx xxxx xxxx xxxx xxxx", &bi_size, 
		   &width, &height, &planes, &bit_count)) {
    *error = ICOERR_Short_File;
    return NULL;
  }

  /* the bitmapinfoheader height includes the height of 
     the and and xor masks */
  if (bi_size != 40 || width != im->width || height != im->height * 2
      || planes != 1) { /* don't know how to handle planes != 1 */
    *error = ICOERR_Invalid_File;
    return NULL;
  }

  result = malloc(sizeof(ico_image_t));
  if (!result) {
    *error = ICOERR_Out_Of_Memory;
    return NULL;
  }
  result->width = width;
  result->height = im->height;
  result->direct = bit_count > 8;
  result->bit_count = bit_count;
  result->palette = NULL;
  result->image_data = NULL;
  result->mask_data = NULL;
    
  if (result->direct) {
    result->palette_size = 0;

    result->image_data = malloc(result->width * result->height * sizeof(ico_color_t));
    if (!result->image_data) {
      free(result);
      *error = ICOERR_Out_Of_Memory;
      return NULL;
    }
    if (bit_count == 32) {
      if (!read_32bit_data(file, result, error)) {
	free(result->image_data);
	free(result);
	return NULL;
      }
    }
    else {
      *error = ICOERR_Unknown_Bits;
      free(result->image_data);
      free(result);
      return NULL;
    }
  }
  else {
    int read_result;
    result->palette_size = 1 << bit_count;
    result->palette = malloc(sizeof(ico_color_t) * result->palette_size);
    result->image_data = malloc(result->width * result->height);
    if (!result->palette) {
      free(result);
      *error = ICOERR_Out_Of_Memory;
      return NULL;
    }

    if (!read_palette(file, result, error)) {
      free(result->palette);
      free(result->image_data);
      free(result);
      return 0;
    }

    switch (bit_count) {
    case 1:
      read_result = read_1bit_data(file, result, error);
      break;

    case 4:
      read_result = read_4bit_data(file, result, error);
      break;
      
    case 8:
      read_result = read_8bit_data(file, result, error);
      break;

    default:
      read_result = 0;
      *error = ICOERR_Unknown_Bits;
      break;
    }

    if (!read_result) {
      free(result->palette);
      free(result->image_data);
      free(result);
      return 0;
    }
  }

  result->mask_data = malloc(result->width * result->height);
  if (!result->mask_data) {
    *error = ICOERR_Out_Of_Memory;
    free(result->palette);
    free(result->image_data);
    free(result);
    return 0;
  }

  if (!read_mask(file, result, error)) {
    free(result->mask_data);
    free(result->palette);
    free(result->image_data);
    free(result);
    return 0;
  }

  return result;
}

/*
=item ico_image_release

Release an image structure returned by ico_image_read.

=cut
*/

void
ico_image_release(ico_image_t *image) {
  free(image->mask_data);
  free(image->palette);
  free(image->image_data);
  free(image);
}

/*
=item ico_reader_close

Releases the file structure.

=cut
*/

void
ico_reader_close(ico_reader_t *file) {
  i_io_close(file->ig);
  free(file->images);
  free(file);
}

/*
=item ico_error_message

Converts an error code into an error message.

=cut
*/

size_t
ico_error_message(int error, char *buffer, size_t buffer_size) {
  char const *msg;
  size_t size;

  switch (error) {
  case ICOERR_Short_File:
    msg = "Short read";
    break;

  case ICOERR_File_Error:
    msg = "I/O error";
    break;

  case ICOERR_Invalid_File:
    msg = "Not an icon file";
    break;

  case ICOERR_Unknown_Bits:
    msg = "Unknown value for bits/pixel";
    break;

  case ICOERR_Bad_Image_Index:
    msg = "Image index out of range";
    break;

  case ICOERR_Out_Of_Memory:
    msg = "Out of memory";
    break;

  default:
    msg = "Unknown error code";
    break;
  }

  size = strlen(msg) + 1;
  if (size > buffer_size)
    size = buffer_size;
  memcpy(buffer, msg, size);
  buffer[size-1] = '\0';

  return size;
}

/*
=back

=head1 PRIVATE FUNCTIONS

=over

=item read_packed

Reads packed data from a stream, unpacking it.

=cut
*/

static
int read_packed(io_glue *ig, const char *format, ...) {
  unsigned char buffer[100];
  va_list ap;
  long *p;
  int size;
  const char *formatp;
  unsigned char *bufp;

  /* read efficiently, work out the size of the buffer */
  size = 0;
  formatp = format;
  while (*formatp) {
    switch (*formatp++) {
    case 'b': 
    case 'x': size += 1; break;
    case 'w': size += 2; break;
    case 'd': size += 4; break;
    case ' ': break; /* space to separate components */
    default:
      fprintf(stderr, "invalid unpack char in %s\n", format);
      exit(1);
    }
  }

  if (size > sizeof(buffer)) {
    /* catch if we need a bigger buffer, but 100 is plenty */
    fprintf(stderr, "format %s too long for buffer\n", format);
    exit(1);
  }

  if (i_io_read(ig, buffer, size) != size) {
    return 0;
  }

  va_start(ap, format);

  bufp = buffer;
  while (*format) {

    switch (*format) {
    case 'b':
      p = va_arg(ap, long *);
      *p = *bufp++;
      break;

    case 'w':
      p = va_arg(ap, long *);
      *p = bufp[0] + (bufp[1] << 8);
      bufp += 2;
      break;

    case 'd':
      p = va_arg(ap, long *);
      *p = bufp[0] + (bufp[1] << 8) + (bufp[2] << 16) + (bufp[3] << 24);
      bufp += 4;
      break;

    case 'x':
      ++bufp; /* skip a byte */
      break;

    case ' ':
      /* nothing to do */
      break;
    }
    ++format;
  }
  return 1;
}

/*
=item read_palette

Reads the palette data for an icon image.

=cut
*/

static
int
read_palette(ico_reader_t *file, ico_image_t *image, int *error) {
  int palette_bytes = image->palette_size * 4;
  unsigned char *read_buffer = malloc(palette_bytes);
  unsigned char *inp;
  ico_color_t *outp;
  int i;

  if (!read_buffer) {
    *error = ICOERR_Out_Of_Memory;
    return 0;
  }

  if (i_io_read(file->ig, read_buffer, palette_bytes) != palette_bytes) {
    *error = ICOERR_Short_File;
    free(read_buffer);
    return 0;
  }

  inp = read_buffer;
  outp = image->palette;
  for (i = 0; i < image->palette_size; ++i) {
    outp->b = *inp++;
    outp->g = *inp++;
    outp->r = *inp++;
    outp->a = 255;
    ++inp;
    ++outp;
  }
  free(read_buffer);

  return 1;
}

/*
=item read_32bit_data

Reads 32 bit image data.

=cut
*/

static
int
read_32bit_data(ico_reader_t *file, ico_image_t *image, int *error) {
  int line_bytes = image->width * 4;
  unsigned char *buffer = malloc(line_bytes);
  int y;
  int x;
  unsigned char *inp;
  ico_color_t *outp;

  if (!buffer) {
    *error = ICOERR_Out_Of_Memory;
    return 0;
  }

  for (y = image->height - 1; y >= 0; --y) {
    if (i_io_read(file->ig, buffer, line_bytes) != line_bytes) {
      free(buffer);
      *error = ICOERR_Short_File;
      return 0;
    }
    outp = image->image_data;
    outp += y * image->width;
    inp = buffer;
    for (x = 0; x < image->width; ++x) {
      outp->b = inp[0];
      outp->g = inp[1];
      outp->r = inp[2];
      outp->a = inp[3];
      ++outp;
      inp += 4;
    }
  }
  free(buffer);

  return 1;
}

/*
=item read_8bit_data

Reads 8 bit image data.

=cut
*/

static
int
read_8bit_data(ico_reader_t *file, ico_image_t *image, int *error) {
  int line_bytes = (image->width + 3) / 4 * 4;
  unsigned char *buffer = malloc(line_bytes);
  int y;
  int x;
  unsigned char *inp, *outp;

  if (!buffer) {
    *error = ICOERR_Out_Of_Memory;
    return 0;
  }

  for (y = image->height - 1; y >= 0; --y) {
    outp = image->image_data;
    outp += y * image->width;
    if (i_io_read(file->ig, buffer, line_bytes) != line_bytes) {
      free(buffer);
      *error = ICOERR_Short_File;
      return 0;
    }
    for (x = 0, inp = buffer; x < image->width; ++x) {
      *outp++ = *inp++;
    }
  }
  free(buffer);

  return 1;
}

/*
=item read_4bit_data

Reads 4 bit image data.

=cut
*/

static
int
read_4bit_data(ico_reader_t *file, ico_image_t *image, int *error) {
  /* 2 pixels per byte, rounded up to the nearest dword */
  int line_bytes = ((image->width + 1) / 2 + 3) / 4 * 4;
  unsigned char *read_buffer = malloc(line_bytes);
  int y;
  int x;
  unsigned char *inp, *outp;

  if (!read_buffer) {
    *error = ICOERR_Out_Of_Memory;
    return 0;
  }

  for (y = image->height - 1; y >= 0; --y) {
    if (i_io_read(file->ig, read_buffer, line_bytes) != line_bytes) {
      free(read_buffer);
      *error = ICOERR_Short_File;
      return 0;
    }
    
    outp = image->image_data;
    outp += y * image->width;
    inp = read_buffer;
    for (x = 0; x < image->width; ++x) {
      /* yes, this is kind of ugly */
      if (x & 1) {
	*outp++ = *inp++ & 0x0F;
      }
      else {
	*outp++ = *inp >> 4;
      }
    }
  }
  free(read_buffer);

  return 1;
}

/*
=item read_1bit_data

Reads 1 bit image data.

=cut
*/

static
int
read_1bit_data(ico_reader_t *file, ico_image_t *image, int *error) {
  /* 8 pixels per byte, rounded up to the nearest dword */
  int line_bytes = ((image->width + 7) / 8 + 3) / 4 * 4;
  unsigned char *read_buffer = malloc(line_bytes);
  int y;
  int x;
  unsigned char *inp, *outp;

  if (!read_buffer) {
    *error = ICOERR_Out_Of_Memory;
    return 0;
  }

  for (y = image->height - 1; y >= 0; --y) {
    if (i_io_read(file->ig, read_buffer, line_bytes) != line_bytes) {
      free(read_buffer);
      *error = ICOERR_Short_File;
      return 0;
    }
    
    outp = image->image_data;
    outp += y * image->width;
    inp = read_buffer;
    for (x = 0; x < image->width; ++x) {
      *outp++ = (*inp >> (7 - (x & 7))) & 1;
      if ((x & 7) == 7)
	++inp;
    }
  }
  free(read_buffer);

  return 1;
}

/* this is very similar to the 1 bit reader <sigh> */
/*
=item read_mask

Reads the AND mask from an icon image.

=cut
*/

static
int
read_mask(ico_reader_t *file, ico_image_t *image, int *error) {
  /* 8 pixels per byte, rounded up to the nearest dword */
  int line_bytes = ((image->width + 7) / 8 + 3) / 4 * 4;
  unsigned char *read_buffer = malloc(line_bytes);
  int y;
  int x;
  unsigned char *inp, *outp;

  if (!read_buffer) {
    *error = ICOERR_Out_Of_Memory;
    return 0;
  }

  for (y = image->height - 1; y >= 0; --y) {
    if (i_io_read(file->ig, read_buffer, line_bytes) != line_bytes) {
      free(read_buffer);
      *error = ICOERR_Short_File;
      return 0;
    }
    
    outp = image->mask_data + y * image->width;
    inp = read_buffer;
    for (x = 0; x < image->width; ++x) {
      *outp++ = (*inp >> (7 - (x & 7))) & 1;
      if ((x & 7) == 7)
	++inp;
    }
  }
  free(read_buffer);

  return 1;
}

/*
=back

=head1 AUTHOR

Tony Cook <tony@imager.perl.org>

=head1 REVISION

$Revision$

=cut
*/
