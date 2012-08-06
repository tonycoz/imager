/*
=head1 NAME

limits.c - manages data/functions for limiting the sizes of images read from files.

=head1 SYNOPSIS

  // user code
  if (!i_set_image_file_limits(max_width, max_height, max_bytes)) {
    // error
  }
  i_get_image_file_limits(&max_width, &max_height, &max_bytes);

  // file reader implementations
  if (!i_int_check_image_file_limits(width, height, channels, sizeof(i_sample_t))) {
    // error handling
  }

=head1 DESCRIPTION

Manage limits for image files read by Imager.

Setting a value of zero means that limit will be ignored.
  
 */

#include "imageri.h"

/*
=item i_set_image_file_limits(width, height, bytes)

=category Files
=synopsis i_set_image_file_limits(500, 500, 1000000);

Set limits on the sizes of images read by Imager.

Setting a limit to 0 means that limit is ignored.

Negative limits result in failure.

Parameters:

=over

=item *

i_img_dim width, height - maximum width and height.

=item *

size_t bytes - maximum size in memory in bytes.  A value of zero sets
this limit to one gigabyte.

=back

Returns non-zero on success.

=cut
*/

int
im_set_image_file_limits(im_context_t ctx, i_img_dim width, i_img_dim height, size_t bytes) {
  i_clear_error();

  if (width < 0) {
    i_push_error(0, "width must be non-negative");
    return 0;
  }
  if (height < 0) {
    i_push_error(0, "height must be non-negative");
    return 0;
  }
  if (bytes < 0) {
    i_push_error(0, "bytes must be non-negative");
    return 0;
  }

  ctx->max_width = width;
  ctx->max_height = height;
  ctx->max_bytes = bytes ? bytes : DEF_BYTES_LIMIT;

  return 1;
}

/*
=item i_get_image_file_limits(&width, &height, &bytes)

=category Files
=synopsis i_get_image_file_limits(&width, &height, &bytes)

Retrieves the file limits set by i_set_image_file_limits().

=over

=item *

i_img_dim *width, *height - the maximum width and height of the image.

=item *

size_t *bytes - size in memory of the image in bytes.

=back

=cut
*/

int
im_get_image_file_limits(im_context_t ctx, i_img_dim *width, i_img_dim *height, size_t *bytes) {
  im_clear_error(ctx);

  *width = ctx->max_width;
  *height = ctx->max_height;
  *bytes = ctx->max_bytes;

  return 1;
}

/*
=item i_int_check_image_file_limits(width, height, channels, sample_size)

=category Files
=synopsis i_i_int_check_image_file_limits(width, height, channels, sizeof(i_sample_t))

Checks the size of a file in memory against the configured image file
limits.

This also range checks the values to those permitted by Imager and
checks for overflows in calculating the size.

Returns non-zero if the file is within limits.

This function is intended to be called by image file read functions.

=cut
*/

int
im_int_check_image_file_limits(im_context_t ctx, i_img_dim width, i_img_dim height, int channels, size_t sample_size) {
  size_t bytes;
  im_clear_error(ctx);
  
  if (width <= 0) {
    im_push_errorf(ctx, 0, "file size limit - image width of %" i_DF " is not positive",
		  i_DFc(width));
    return 0;
  }
  if (ctx->max_width && width > ctx->max_width) {
    im_push_errorf(ctx, 0, "file size limit - image width of %" i_DF " exceeds limit of %" i_DF,
		  i_DFc(width), i_DFc(ctx->max_width));
    return 0;
  }

  if (height <= 0) {
    im_push_errorf(ctx, 0, "file size limit - image height of %" i_DF " is not positive",
		  i_DFc(height));
    return 0;
  }

  if (ctx->max_height && height > ctx->max_height) {
    im_push_errorf(ctx, 0, "file size limit - image height of %" i_DF
		  " exceeds limit of %" i_DF, i_DFc(height), i_DFc(ctx->max_height));
    return 0;
  }

  if (channels < 1 || channels > MAXCHANNELS) {
    im_push_errorf(ctx, 0, "file size limit - channels %d out of range",
		  channels);
    return 0;
  }
  
  if (sample_size < 1 || sample_size > sizeof(long double)) {
    im_push_errorf(ctx, 0, "file size limit - sample_size %ld out of range",
		  (long)sample_size);
    return 0;
  }

  /* This overflow check is a bit more paranoid than usual.
     We don't protect it under max_bytes since we always want to check 
     for overflow.
  */
  bytes = width * height * channels * sample_size;
  if (bytes / width != height * channels * sample_size
      || bytes / height != width * channels * sample_size) {
    i_push_error(0, "file size limit - integer overflow calculating storage");
    return 0;
  }
  if (ctx->max_bytes) {
    if (bytes > ctx->max_bytes) {
      im_push_errorf(ctx, 0, "file size limit - storage size of %lu "
		    "exceeds limit of %lu", (unsigned long)bytes,
		    (unsigned long)ctx->max_bytes);
      return 0;
    }
  }

  return 1;
}
