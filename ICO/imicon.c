#include "imext.h"
#include "imicon.h"
#include "msicon.h"

static void
ico_push_error(int error) {
  char error_buf[ICO_MAX_MESSAGE];

  ico_error_message(error, error_buf, sizeof(error_buf));
  i_push_error(error, error_buf);
}

static
i_img *
read_one_icon(ico_reader_t *file, int index) {
  ico_image_t *image;
  int error;
  i_img *result;

  image = ico_image_read(file, index, &error);
  if (!image) {
    ico_reader_close(file);
    ico_push_error(error);
    return NULL;
  }

  if (image->direct) {
    int x, y;
    i_color *line_buf = mymalloc(image->width * sizeof(i_color));
    i_color *outp;
    ico_color_t *inp = image->image_data;

    result = i_img_8_new(image->width, image->height, 4);
    if (!result) {
      ico_image_release(image);
      ico_reader_close(file);
      return NULL;
    }

    for (y = 0; y < image->height; ++y) {
      outp = line_buf;
      for (x = 0; x < image->width; ++x) {
	outp->rgba.r = inp->r;
	outp->rgba.g = inp->g;
	outp->rgba.b = inp->b;
	outp->rgba.a = inp->a;
	++outp;
	++inp;
      }
      i_plin(result, 0, image->width, y, line_buf);
    }

    myfree(line_buf);
  }
  else {
    int pal_index;
    int y;
    unsigned char *image_data;

    result = i_img_pal_new(image->width, image->height, 3, 256);
    if (!result) {
      ico_image_release(image);
      ico_reader_close(file);
      return NULL;
    }
    
    /* fill in the palette */
    for (pal_index = 0; pal_index < image->palette_size; ++pal_index) {
      i_color c;
      c.rgba.r = image->palette[pal_index].r;
      c.rgba.g = image->palette[pal_index].g;
      c.rgba.b = image->palette[pal_index].b;
      c.rgba.a = 255; /* so as to not confuse some code */

      if (i_addcolors(result, &c, 1) < 0) {
	i_push_error(0, "could not add color to palette");
	ico_image_release(image);
	ico_reader_close(file);
	i_img_destroy(result);
	return NULL;
      }
    }

    /* fill in the image data */
    image_data = image->image_data;
    for (y = 0; y < image->height; ++y) {
      i_ppal(result, 0, image->width, y, image_data);
      image_data += image->width;
    }
  }

  {
    unsigned char *inp = image->mask_data;
    char *outp;
    int x, y;
    char *mask;
    /* fill in the mask tag */
    /* space for " .\n", width + 1 chars per line and NUL */
    mask = mymalloc(3 + (image->width + 1) * image->height + 1);

    outp = mask;
    *outp++ = '.';
    *outp++ = '*';
    *outp++ = '\n';
    for (y = 0; y < image->height; ++y) {
      for (x = 0; x < image->width; ++x) {
	*outp++ = *inp++ ? '*' : '.';
      }
      if (y != image->height - 1) /* not on the last line */
	*outp++ = '\n';
    }
    *outp++ = '\0';

    i_tags_set(&result->tags, "ico_mask", mask, (outp-mask)-1);
    
    myfree(mask);
  }
  i_tags_setn(&result->tags, "ico_bits", image->bit_count);
  i_tags_set(&result->tags, "ico_type", ico_type(file) == ICON_ICON ? "icon" : "cursor", -1);
  i_tags_set(&result->tags, "i_format", "ico", 3);

  ico_image_release(image);

  return result;
}

i_img *
i_readico_single(io_glue *ig, int index) {
  ico_reader_t *file;
  i_img *result;
  int error;

  file = ico_reader_open(ig, &error);
  if (!file) {
    ico_push_error(error);
    return NULL;
  }

  if (index < 0 && index >= ico_image_count(file)) {
    i_push_error(0, "page out of range");
    return NULL;
  }

  result = read_one_icon(file, index);
  ico_reader_close(file);

  return result;
}

i_img **
i_readico_multi(io_glue *ig, int *count) {
  ico_reader_t *file;
  int index;
  int error;
  i_img **imgs;

  file = ico_reader_open(ig, &error);
  if (!file) {
    ico_push_error(error);
    return NULL;
  }

  imgs = mymalloc(sizeof(i_img *) * ico_image_count(file));

  *count = 0;
  for (index = 0; index < ico_image_count(file); ++index) {
    i_img *im = read_one_icon(file, index);
    if (!im) 
      break;

    imgs[(*count)++] = im;
  }

  ico_reader_close(file);

  if (*count == 0) {
    myfree(imgs);
    return NULL;
  }

  return imgs;
}
