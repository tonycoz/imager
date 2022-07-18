/* 
   Imager "functions" implemented as macros 

   I suppose these could go in imdatatypes, but they aren't types.
*/
#ifndef IMAGER_IMMACROS_H_
#define IMAGER_IMMACROS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
=item i_img_has_alpha(C<im>)

=category Image Information

Return true if the image has an alpha channel.

=cut
*/

#define i_img_has_alpha(im) (i_img_alpha_channel((im), NULL))

/*
=item i_psamp(im, left, right, y, samples, channels, channel_count)
=category Drawing

Writes sample values from C<samples> to C<im> for the horizontal line
(left, y) to (right-1, y) inclusive for the channels specified by
C<channels>, an array of C<int> with C<channel_count> elements.

If C<channels> is C<NULL> then the first C<channels_count> channels
are written to for each pixel.

Returns the number of samples written, which should be (right - left)
* channel_count.  If a channel not in the image is in channels, left
is negative, left is outside the image or y is outside the image,
returns -1 and pushes an error.

=cut
*/

#define i_psamp(im, l, r, y, samps, chans, count) \
  (((im)->vtbl->i_f_psamp)((im), (l), (r), (y), (samps), (chans), (count)))

/*
=item i_psampf(im, left, right, y, samples, channels, channel_count)
=category Drawing

Writes floating point sample values from C<samples> to C<im> for the
horizontal line (left, y) to (right-1, y) inclusive for the channels
specified by C<channels>, an array of C<int> with C<channel_count>
elements.

If C<channels> is C<NULL> then the first C<channels_count> channels
are written to for each pixel.

Returns the number of samples written, which should be (right - left)
* channel_count.  If a channel not in the image is in channels, left
is negative, left is outside the image or y is outside the image,
returns -1 and pushes an error.

=cut
*/

#define i_psampf(im, l, r, y, samps, chans, count) \
  (((im)->vtbl->i_f_psampf)((im), (l), (r), (y), (samps), (chans), (count)))

/*
=item i_img_data()

  i_img_data(im, layout, bits, flags, &ptr, &size, &extrachannels)

Returns raw bytes representing the image.

Typical use is something like:

  // image data I can write to a file
  void *data;
  size_t size;
  i_image_alloc_t *alloc = i_img_data(img, idf_rgb, i_8_bits, idf_synthesize, &data, &size, NULL);
  if (alloc) {
    // write to some file
    ...
    i_img_data_release(alloc);
  }

  // image data I can modify to modify the image
  void *data;
  size_t size;
  i_image_alloc_t *alloc = i_img_data(img, idf_rgb, i_8_bits, idf_writable, &data, &size, NULL);
  if (alloc) {
    // modify the image data
    ...
    i_img_data_release(alloc);
  }

Note that even without C<idf_synthesize> the data pointer should be
treated as pointing at read only data, unless C<idf_writable> is set
in C<flags>.

Parameters:

=over

=item * C<img> - the image to return image data for

=item *

C<layout> - the desired image layout.  Note that the typical Imager
layouts are C<idf_palette> through C<idf_rgb_alpha>.  The numeric
values of C<idf_gray> through C<idf_rgb_alpha> correspond to the
Imager channel counts, but it's possible for third-party images (of
which none exist at this point) may have another layout.

=item *

C<bits> - the sample size for the images.  This can be any of
C<i_8_bits>, C<i_16_bits> or C<i_double_bits>.  Other sample sizes may
be added.

=item *

C<flags> - a bit combination of the following:

=over

=item *

C<idf_writable> - modifying samples pointed at will modify the image.
Without this flag the data should be treated as read only (and this
may be enforced.)

=item *

C<idf_synthesize> - if the native image data doesn't match the
requested format, Imager will allocate memory and synthesize the
layout requested.  Some layouts are not supported for synthesis
including C<idf_palette> in any case and C<idf_gray> or
C<idf_gray_alpha> from any RGB layout.

=item *

C<idf_extras> - allows for the original image data to be returned,
ie. for non-synthesizes or writable data even if the image has extra
channels stored for pixel.

=back

=item *

C<&data> - a pointer to C<void *> which is filled with a pointer to
the image data.

=item *

C<&size> - a pointer to C<size_t> which is filled with the size of the
image data in bytes.  This is intended for validating the result.

=item *

C<&extrachannels> - a pointer to C<int> which will be filled with the
number of extra channels in the image.  This pointer may be C<NULL> if
the C<idf_extras> flag isn't set.  Extra channels are currently not
implemented and this will always be set to zero.

=back

=cut
*/

#define i_img_data(im, layout, bits, flags, pptr, psize, pextra) \
  (((im)->vtbl->i_f_data)((im), (layout), (bits), (flags), (pptr), (psize), (pextra)))

/*
=item i_img_data_release()

Releases the allocation structure and any associated resources
returned from i_img_data().

  i_img_data_release(allocation);

This can safely accept a NULL pointer.

=cut
*/

#define i_img_data_release(alloc) ((alloc) ? (( ((i_image_data_alloc_t *)(alloc))->f_release)(alloc)) : (void)0)

#ifndef IMAGER_DIRECT_IMAGE_CALLS
#define IMAGER_DIRECT_IMAGE_CALLS 1
#endif

#if IMAGER_DIRECT_IMAGE_CALLS

#define i_ppix(im, x, y, val) \
  ((i_psamp((im), (x), (x)+1, (y), (val)->channel, NULL, (im)->channels) > 0) ? 0 : -1)

#define i_ppixf(im, x, y, val) \
  ((i_psampf((im), (x), (x)+1, (y), (val)->channel, NULL, (im)->channels) > 0) ? 0 : -1)
#define i_gsamp(im, l, r, y, samps, chans, count) \
  (((im)->vtbl->i_f_gsamp)((im), (l), (r), (y), (samps), (chans), (count)))
#define i_gsampf(im, l, r, y, samps, chans, count) \
  (((im)->vtbl->i_f_gsampf)((im), (l), (r), (y), (samps), (chans), (count)))

#endif

#define i_gsamp_bits(im, l, r, y, samps, chans, count, bits) \
  (((im)->vtbl->i_f_gsamp_bits)((im), (l), (r), (y), (samps), (chans), (count), (bits)))
#define i_psamp_bits(im, l, r, y, samps, chans, count, bits) \
       (((im)->vtbl->i_f_psamp_bits)((im), (l), (r), (y), (samps), (chans), (count), (bits)))

#define i_findcolor(im, color, entry) \
  (((im)->vtbl->i_f_findcolor) ? ((im)->vtbl->i_f_findcolor)((im), (color), (entry)) : 0)

#define i_gpal(im, l, r, y, vals) \
  (((im)->vtbl->i_f_gpal) ? ((im)->vtbl->i_f_gpal)((im), (l), (r), (y), (vals)) : 0)
#define i_ppal(im, l, r, y, vals) \
  (((im)->vtbl->i_f_ppal) ? ((im)->vtbl->i_f_ppal)((im), (l), (r), (y), (vals)) : 0)
#define i_addcolors(im, colors, count) \
  (((im)->vtbl->i_f_addcolors) ? ((im)->vtbl->i_f_addcolors)((im), (colors), (count)) : -1)
#define i_getcolors(im, index, color, count) \
  (((im)->vtbl->i_f_getcolors) ? \
   ((im)->vtbl->i_f_getcolors)((im), (index), (color), (count)) : 0)
#define i_setcolors(im, index, color, count) \
  (((im)->vtbl->i_f_setcolors) ? \
   ((im)->vtbl->i_f_setcolors)((im), (index), (color), (count)) : 0)
#define i_colorcount(im) \
  (((im)->vtbl->i_f_colorcount) ? ((im)->vtbl->i_f_colorcount)(im) : -1)
#define i_maxcolors(im) \
  (((im)->vtbl->i_f_maxcolors) ? ((im)->vtbl->i_f_maxcolors)(im) : -1)
#define i_findcolor(im, color, entry) \
  (((im)->vtbl->i_f_findcolor) ? ((im)->vtbl->i_f_findcolor)((im), (color), (entry)) : 0)

/*
=item i_img_refcnt()

Return the reference count for the image.

=cut
*/

#define i_img_virtual(im) ((im)->isvirtual)
#define i_img_type(im) ((im)->type)
#define i_img_bits(im) ((im)->bits)
#define i_img_extrachannels(im) ((im)->extrachannels)
#define i_img_totalchannels(im) ((im)->channels + (im)->extrachannels)
#define i_img_refcnt(im) (0+(im)->ref_count)

#define pIMCTX im_context_t my_im_ctx

#ifdef IMAGER_NO_CONTEXT
#define dIMCTXctx(ctx) pIMCTX = (ctx)
#define dIMCTX dIMCTXctx(im_get_context())
#define dIMCTXim(im) dIMCTXctx((im)->context)
#define dIMCTXio(io) dIMCTXctx((io)->context)
#define aIMCTX my_im_ctx
#else
#define aIMCTX im_get_context()
#endif

#define i_img_8_new(xsize, ysize, channels) im_img_8_new(aIMCTX, (xsize), (ysize), (channels))
#define i_img_16_new(xsize, ysize, channels) im_img_16_new(aIMCTX, (xsize), (ysize), (channels))
#define i_img_double_new(xsize, ysize, channels) im_img_double_new(aIMCTX, (xsize), (ysize), (channels))

#define i_img_8_new_extra(xsize, ysize, channels, extra)                      \
  im_img_8_new_extra(aIMCTX, (xsize), (ysize), (channels), (extra))
#define i_img_16_new_extra(xsize, ysize, channels, extra)                     \
  im_img_16_new_extra(aIMCTX, (xsize), (ysize), (channels), (extra))
#define i_img_double_new_extra(xsize, ysize, channels, extra)                 \
  im_img_double_new_extra(aIMCTX, (xsize), (ysize), (channels), (extra))

#define i_img_pal_new(xsize, ysize, channels, maxpal) \
  im_img_pal_new(aIMCTX, (xsize), (ysize), (channels), (maxpal))

#define i_img_alloc() im_img_alloc(aIMCTX)
#define i_img_init(im) im_img_init(aIMCTX, im)

#define i_set_image_file_limits(width, height, bytes) im_set_image_file_limits(aIMCTX, width, height, bytes)
#define i_get_image_file_limits(width, height, bytes) im_get_image_file_limits(aIMCTX, width, height, bytes)
#define i_int_check_image_file_limits(width, height, channels, sample_size) im_int_check_image_file_limits(aIMCTX, width, height, channels, sample_size)

#define i_clear_error() im_clear_error(aIMCTX)
#define i_push_errorvf(code, fmt, args) im_push_errorvf(aIMCTX, code, fmt, args)
#define i_push_error(code, msg) im_push_error(aIMCTX, code, msg)
#define i_errors() im_errors(aIMCTX)

#define io_new_fd(fd) im_io_new_fd(aIMCTX, (fd))
#define io_new_bufchain() im_io_new_bufchain(aIMCTX)
#define io_new_buffer(data, len, closecb, closectx) im_io_new_buffer(aIMCTX, (data), (len), (closecb), (closectx))
#define io_new_cb(p, readcb, writecb, seekcb, closecb, destroycb) \
  im_io_new_cb(aIMCTX, (p), (readcb), (writecb), (seekcb), (closecb), (destroycb))

#define i_io_set_max_mmap_size(size) im_io_set_max_mmap_size(aIMCTX, (size))
#define i_io_get_max_mmap_size()     im_io_get_max_mmap_size(aIMCTX)

/*
=item IM_DEPRECATED(name)

Expands to an attribute on supported compilers that causes the
compiler to produce a deprecation warning when the given function is
called.

  int foo(void) IM_DEPRECATED(foo);

=item IM_DEPRECATED_MACRO(name)

Intended for use in macros to warn that the given macro is deprecated.

  #define foo() (IM_DEPRECATED_MACRO(foo), code for foo)

=cut
*/

#if defined(__GNUC__) || defined(__clang__)

#define IM_DEPRECATED(name) __attribute__((deprecated))
#define IM_DO_PRAGMA(x)     (_Pragma #x)
#define IM_DEPRECATED_MACRO(name)    IM_DO_PRAGMA(GCC warning (#name " is deprecated"))

#else

#define IM_DEPRECATED(name)
#define IM_DEPRECATED_MACRO(name)    ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif
