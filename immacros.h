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
=item pIMCTX
=category Context objects

Declares a context object, similar to C<dTHX> in perl XS, except that
it is non-conditional, so there is no C<pIMCTX_>.

=cut
*/

#define pIMCTX im_context_t my_im_ctx

/*
=item i_assert_valid_channels(im, chans, chan_count)

This is used by image get/set sample implementations to assert that
C<chans>/C<chan_count> paremeters are valid.

=cut
*/

#define i_assert_valid_channels(im, chans, chan_count) \
  assert(i_img_valid_chans_assert((im), (chans), (chan_count)))

/*
=item dIMCTXctx(context)
=category Context objects

Defines and initializes a local context object pointer from C<ctx>.

Used to define the other macros such as dIMCTX, dIMCTXim() and
dIMCTXio() which you probably want to use instead.

Only available when C<IMAGER_NO_CONTEXT> is defined.

=item dIMCTX
=category Context objects

Defines and initializes a context object pointer from TLS.

Only available when C<IMAGER_NO_CONTEXT> is defined.

=item dIMCTXim(im)
=category Context objects

Defines and initializes a context object pointer from the context
pointer in an image.

Only available when C<IMAGER_NO_CONTEXT> is defined.

=item dIMCTXio(io)
=category Context objects

Defines and initializes a context object pointer from the context
pointer in an C<io_glue> object.

Only available when C<IMAGER_NO_CONTEXT> is defined.

=item aIMCTX
=category Context objects

The imager context object.

When C<IMAGER_NO_CONTEXT> is defined this expects a C<dIMCTX*> or
C<pIMCTX> in scope.

When C<IMAGER_NO_CONTEXT> is not defined, this directly calls the
context fetch function.

Unlike the perl XS C<aTHX> there is no C<aIMCTX_> since Imager always
uses the context object.

=cut
*/

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
#define i_img_float_new(xsize, ysize, channels) im_img_float_new(aIMCTX, (xsize), (ysize), (channels))

#define i_lin_img_16_new(xsize, ysize, channels) im_lin_img_16_new(aIMCTX, (xsize), (ysize), (channels))
#define i_lin_img_double_new(xsize, ysize, channels) im_lin_img_double_new(aIMCTX, (xsize), (ysize), (channels))

#define i_img_8_new_extra(xsize, ysize, channels, extra)                      \
  im_img_8_new_extra(aIMCTX, (xsize), (ysize), (channels), (extra))
#define i_img_16_new_extra(xsize, ysize, channels, extra)                     \
  im_img_16_new_extra(aIMCTX, (xsize), (ysize), (channels), (extra))
#define i_img_double_new_extra(xsize, ysize, channels, extra)                 \
  im_img_double_new_extra(aIMCTX, (xsize), (ysize), (channels), (extra))
#define i_img_float_new_extra(xsize, ysize, channels, extra)                 \
  im_img_float_new_extra(aIMCTX, (xsize), (ysize), (channels), (extra))

#define i_lin_img_16_new_extra(xsize, ysize, channels, extra)                     \
  im_lin_img_16_new_extra(aIMCTX, (xsize), (ysize), (channels), (extra))
#define i_lin_img_double_new_extra(xsize, ysize, channels, extra)                     \
  im_lin_img_double_new_extra(aIMCTX, (xsize), (ysize), (channels), (extra))

#define i_img_pal_new(xsize, ysize, channels, maxpal) \
  im_img_pal_new(aIMCTX, (xsize), (ysize), (channels), (maxpal))

#define i_img_alloc() im_img_alloc(aIMCTX)
#define i_img_init(im) im_img_init(aIMCTX, im)
#define i_img_new(vtbl, xsize, ysize, channels, extra, bits) \
  im_img_new(aIMCTX, (vtbl), (xsize), (ysize), (channels), (extra), (bits))

#define i_set_image_file_limits(width, height, bytes) im_set_image_file_limits(aIMCTX, width, height, bytes)
#define i_get_image_file_limits(width, height, bytes) im_get_image_file_limits(aIMCTX, width, height, bytes)
#define i_int_check_image_file_limits(width, height, channels, sample_size) im_int_check_image_file_limits(aIMCTX, width, height, channels, sample_size)

#define i_clear_error() im_clear_error(aIMCTX)
#define i_push_errorvf(code, fmt, args) im_push_errorvf(aIMCTX, code, fmt, args)
#define i_push_error(code, msg) im_push_error(aIMCTX, code, msg)
#define i_errors() im_errors(aIMCTX)

/*
=item io_new_fd(fd)
=category I/O Layers

Exactly equivalent to:

 im_io_new_fd(aIMCTX, fd);

See L</im_io_new_fd(ctx, file)>.

=cut
*/

#define io_new_fd(fd) im_io_new_fd(aIMCTX, (fd))
#define io_new_bufchain() im_io_new_bufchain(aIMCTX)
#define io_new_buffer(data, len, closecb, closectx) im_io_new_buffer(aIMCTX, (data), (len), (closecb), (closectx))
#define io_new_cb(p, readcb, writecb, seekcb, closecb, destroycb) \
  im_io_new_cb(aIMCTX, (p), (readcb), (writecb), (seekcb), (closecb), (destroycb))

#define i_io_set_max_mmap_size(size) im_io_set_max_mmap_size(aIMCTX, (size))
#define i_io_get_max_mmap_size()     im_io_get_max_mmap_size(aIMCTX)

/*
=item i_model_curves(model, pcolor_chans)
=synopsis int color_chans;
=synopsis imcms_curve_t *curves = i_model_curves(icm_gray, &color_chans);

Fetch the color channel curves for each channel in the given color
model.


Exactly equivalent to:

  im_model_curves(aIMCTX, model, pcolor_chans)

See L</im_model_curves(ctx, model, pcolor_chans)>.

=cut
*/
#define i_model_curves(model, pcolor_chans) \
  im_model_curves(aIMCTX, (model), (pcolor_chans))

/*
=item IM_DEPRECATED(name)
=category Utility macros

Expands to an attribute on supported compilers that causes the
compiler to produce a deprecation warning when the given function is
called.

  int foo(void) IM_DEPRECATED(foo);

=item IM_DEPRECATED_MACRO(name)
=category Utility macros

Intended for use in macros to warn that the given macro is deprecated.

  #define foo() (IM_DEPRECATED_MACRO(foo), code for foo)

=cut
*/
  
#if defined(__GNUC__) || defined(__clang__)

#define IM_DEPRECATED(name) __attribute__((deprecated))
#define IM_DEPRECATED_MACRO(name) \
  (_Pragma GCC warning (#name " is deprecated"))

#else

#define IM_DEPRECATED(name)
#define IM_DEPRECATED_MACRO(name)    ((void)0)

#endif

/*
=item IM_CAT(x, y)
=category Utility macros

Token pasting - concatenate the macro expansions of C<x> and C<y>.

=item IM_QUOTE(x)
=category Utility macros

Token pasting - quote the macro expansion of C<x>.

=item IM_UNUSED_VAR
=category Utility macros

Used to mention a variable to suppress unused variable or parameter
warnings.

=cut
*/

#define IM_CAT_(x, y) x##y
#define IM_CAT(x, y) IM_CAT_(x, y)

#define IM_QUOTE_(x) #x
#define IM_QUOTE(x) IM_QUOTE_(x)

#define IM_UNUSED_VAR(x) ((void)sizeof(x))
  
#if defined(IM_ASSERT) || defined(DEBUGGING)
#  undef NDEBUG
#else
#  ifndef NDEBUG
#    define NDEBUG
#  endif
#endif
#include <assert.h>

#ifdef __cplusplus
}
#endif

#endif
