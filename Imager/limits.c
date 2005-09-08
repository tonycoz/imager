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

#include "imagei.h"

static int max_width, max_height;
static int max_bytes;

int
i_set_image_file_limits(int width, int height, int bytes) {
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

  max_width = width;
  max_height = height;
  max_bytes = bytes;

  return 1;
}

int
i_get_image_file_limits(int *width, int *height, int *bytes) {
  i_clear_error();

  *width = max_width;
  *height = max_height;
  *bytes = max_bytes;

  return 1;
}

int
i_int_check_image_file_limits(int width, int height, int channels, int sample_size) {
  int bytes;
  i_clear_error();
  
  if (width <= 0) {
    i_push_errorf(0, "file size limit - image width of %d is not positive",
		  width);
    return 0;
  }
  if (max_width && width > max_width) {
    i_push_errorf(0, "file size limit - image width of %d exceeds limit of %d",
		 width, max_width);
    return 0;
  }

  if (height <= 0) {
    i_push_errorf(0, "file size limit - image height %d is not positive",
		  height);
    return 0;
  }

  if (max_height && height > max_height) {
    i_push_errorf(0, "file size limit - image height of %d "
		  "exceeds limit of %d", height, max_height);
    return 0;
  }

  if (channels < 1 || channels > MAXCHANNELS) {
    i_push_errorf(0, "file size limit - channels %d out of range",
		  channels);
    return 0;
  }
  
  if (sample_size < 1 || sample_size > sizeof(long double)) {
    i_push_errorf(0, "file size limit - sample_size %d out of range",
		  sample_size);
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
  if (max_bytes) {
    if (bytes > max_bytes) {
      i_push_errorf(0, "file size limit - storage size of %d "
		    "exceeds limit of %d", bytes, max_bytes);
      return 0;
    }
  }

  return 1;
}
