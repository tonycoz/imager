#include "imexttypes.h"
#include "imager.h"

/*
 DON'T ADD CASTS TO THESE
*/
im_ext_funcs imager_function_table =
  {
    IMAGER_API_VERSION,
    IMAGER_API_LEVEL,

    mymalloc,
    myfree,
    myrealloc,

    mymalloc_file_line,
    myfree_file_line,
    myrealloc_file_line,

    i_img_8_new,
    i_img_16_new,
    i_img_double_new,
    i_img_pal_new,
    i_img_destroy,
    i_sametype,
    i_sametype_chans,
    i_img_info,

    i_ppix,
    i_gpix,
    i_ppixf,
    i_gpixf,
    i_plin,
    i_glin,
    i_plinf,
    i_glinf,
    i_gsamp,
    i_gsampf,
    i_gpal,
    i_ppal,
    i_addcolors,
    i_getcolors,
    i_colorcount,
    i_maxcolors,
    i_findcolor,
    i_setcolors,

    i_new_fill_solid,
    i_new_fill_solidf,
    i_new_fill_hatch,
    i_new_fill_hatchf,
    i_new_fill_image,
    i_new_fill_fount,
    i_fill_destroy,

    i_quant_makemap,
    i_quant_translate,
    i_quant_transparent,

    i_clear_error,
    i_push_error,
    i_push_errorf,
    i_push_errorvf,

    i_tags_new,
    i_tags_set,
    i_tags_setn,
    i_tags_destroy,
    i_tags_find,
    i_tags_findn,
    i_tags_delete,
    i_tags_delbyname,
    i_tags_delbycode,
    i_tags_get_float,
    i_tags_set_float,
    i_tags_set_float2,
    i_tags_get_int,
    i_tags_get_string,
    i_tags_get_color,
    i_tags_set_color,

    i_box,
    i_box_filled,
    i_box_cfill,
    i_line,
    i_line_aa,
    i_arc,
    i_arc_aa,
    i_arc_cfill,
    i_arc_aa_cfill,
    i_circle_aa,
    i_flood_fill,
    i_flood_cfill,

    i_copyto,
    i_copyto_trans,
    i_copy,
    i_rubthru,

    /* IMAGER_API_LEVEL 2 functions */
    i_set_image_file_limits,
    i_get_image_file_limits,
    i_int_check_image_file_limits,

    i_flood_fill_border,
    i_flood_cfill_border,

    /* IMAGER_API_LEVEL 3 functions */
    i_img_setmask,
    i_img_getmask,
    i_img_getchannels,
    i_img_get_width,
    i_img_get_height,
    i_lhead,
    i_loog,

    /* IMAGER_API_LEVEL 4 functions */
    i_img_alloc,
    i_img_init,
  };

/* in general these functions aren't called by Imager internally, but
   only via the pointers above, since faster macros that call the
   image vtable pointers are used.

   () are used around the function names to prevent macro replacement
   on the function names.
*/

/*
=item i_ppix(im, x, y, color)

=category Drawing

Sets the pixel at (x,y) to I<color>.

Returns 0 if the pixel was drawn, or -1 if not.

Does no alpha blending, just copies the channels from the supplied
color to the image.

=cut
*/

int 
(i_ppix)(i_img *im, int x, int y, const i_color *val) {
  return i_ppix(im, x, y, val);
}

/*
=item i_gpix(im, C<x>, C<y>, C<color>)

=category Drawing

Retrieves the C<color> of the pixel (x,y).

Returns 0 if the pixel was retrieved, or -1 if not.

=cut
*/

int
(i_gpix)(i_img *im,int x,int y,i_color *val) {
  return i_gpix(im, x, y, val);
}

/*
=item i_ppixf(im, C<x>, C<y>, C<fcolor>)

=category Drawing

Sets the pixel at (C<x>,C<y>) to the floating point color C<fcolor>.

Returns 0 if the pixel was drawn, or -1 if not.

Does no alpha blending, just copies the channels from the supplied
color to the image.

=cut
*/
int
(i_ppixf)(i_img *im, int x, int y, const i_fcolor *val) {
  return i_ppixf(im, x, y, val);
}

/*
=item i_gpixf(im, C<x>, C<y>, C<fcolor>)

=category Drawing

Retrieves the color of the pixel (x,y) as a floating point color into
C<fcolor>.

Returns 0 if the pixel was retrieved, or -1 if not.

=cut
*/

int
(i_gpixf)(i_img *im,int x,int y,i_fcolor *val) {
  return i_gpixf(im, x, y, val);
}

/*
=item i_plin(im, l, r, y, colors)

=category Drawing

Sets (r-l) pixels starting from (l,y) using (r-l) values from
I<colors>.

Returns the number of pixels set.

=cut
*/

int
(i_plin)(i_img *im, int l, int r, int y, const i_color *vals) {
  return i_plin(im, l, r, y, vals);
}

/*
=item i_glin(im, l, r, y, colors)

=category Drawing

Retrieves (r-l) pixels starting from (l,y) into I<colors>.

Returns the number of pixels retrieved.

=cut
*/

int
(i_glin)(i_img *im, int l, int r, int y, i_color *vals) {
  return i_glin(im, l, r, y, vals);
}

/*
=item i_plinf(im, C<left>, C<right>, C<fcolors>)

=category Drawing

Sets (right-left) pixels starting from (left,y) using (right-left)
floating point colors from C<fcolors>.

Returns the number of pixels set.

=cut
*/

int
(i_plinf)(i_img *im, int l, int r, int y, const i_fcolor *vals) {
  return i_plinf(im, l, r, y, vals);
}

/*
=item i_glinf(im, l, r, y, colors)

=category Drawing

Retrieves (r-l) pixels starting from (l,y) into I<colors> as floating
point colors.

Returns the number of pixels retrieved.

=cut
*/

int
(i_glinf)(i_img *im, int l, int r, int y, i_fcolor *vals) {
  return i_glinf(im, l, r, y, vals);
}

/*
=item i_gsamp(im, left, right, y, samples, channels, channel_count)

=category Drawing

Reads sample values from C<im> for the horizontal line (left, y) to
(right-1,y) for the channels specified by C<channels>, an array of int
with C<channel_count> elements.

If channels is NULL then the first channels_count channels are retrieved for
each pixel.

Returns the number of samples read (which should be (right-left) *
channel_count)

=cut
*/
int
(i_gsamp)(i_img *im, int l, int r, int y, i_sample_t *samp, 
                   const int *chans, int chan_count) {
  return i_gsamp(im, l, r, y, samp, chans, chan_count);
}

/*
=item i_gsampf(im, left, right, y, samples, channels, channel_count)

=category Drawing

Reads floating point sample values from C<im> for the horizontal line
(left, y) to (right-1,y) for the channels specified by C<channels>, an
array of int with channel_count elements.

If C<channels> is NULL then the first C<channel_count> channels are
retrieved for each pixel.

Returns the number of samples read (which should be (C<right>-C<left>)
* C<channel_count>)

=cut
*/
int
(i_gsampf)(i_img *im, int l, int r, int y, i_fsample_t *samp, 
           const int *chans, int chan_count) {
  return i_gsampf(im, l, r, y, samp, chans, chan_count);
}

/*
=item i_gpal(im, left, right, y, indexes)

=category Drawing

Reads palette indexes for the horizontal line (left, y) to (right-1,
y) into C<indexes>.

Returns the number of indexes read.

Always returns 0 for direct color images.

=cut
*/
int
(i_gpal)(i_img *im, int x, int r, int y, i_palidx *vals) {
  return i_gpal(im, x, r, y, vals);
}

/*
=item i_ppal(im, left, right, y, indexes)

=category Drawing

Writes palette indexes for the horizontal line (left, y) to (right-1,
y) from C<indexes>.

Returns the number of indexes written.

Always returns 0 for direct color images.

=cut
*/
int
(i_ppal)(i_img *im, int x, int r, int y, const i_palidx *vals) {
  return i_ppal(im, x, r, y, vals);
}

/*
=item i_addcolors(im, colors, count)

=category Paletted images

Adds colors to the image's palette.

On success returns the index of the lowest color added.

On failure returns -1.

Always fails for direct color images.

=cut
*/

int
(i_addcolors)(i_img *im, const i_color *colors, int count) {
  return i_addcolors(im, colors, count);
}

/*
=item i_getcolors(im, index, colors, count)

=category Paletted images

Retrieves I<count> colors starting from I<index> in the image's
palette.

On success stores the colors into I<colors> and returns true.

On failure returns false.

Always fails for direct color images.

Fails if there are less than I<index>+I<count> colors in the image's
palette.

=cut
*/

int
(i_getcolors)(i_img *im, int i, i_color *colors, int count) {
  return i_getcolors(im, i, colors, count);
}

/*
=item i_colorcount(im)

=category Paletted images

Returns the number of colors in the image's palette.

Returns -1 for direct images.

=cut
*/

int
(i_colorcount)(i_img *im) {
  return i_colorcount(im);
}

/*
=item i_maxcolors(im)

=category Paletted images

Returns the maximum number of colors the palette can hold for the
image.

Returns -1 for direct color images.

=cut
*/

int
(i_maxcolors)(i_img *im) {
  return i_maxcolors(im);
}

/*
=item i_findcolor(im, color, &entry)

=category Paletted images

Searches the images palette for the given color.

On success sets *I<entry> to the index of the color, and returns true.

On failure returns false.

Always fails on direct color images.

=cut
*/
int
(i_findcolor)(i_img *im, const i_color *color, i_palidx *entry) {
  return i_findcolor(im, color, entry);
}

/*
=item i_setcolors(im, index, colors, count)

=category Paletted images

Sets I<count> colors starting from I<index> in the image's palette.

On success returns true.

On failure returns false.

The image must have at least I<index>+I<count> colors in it's palette
for this to succeed.

Always fails on direct color images.

=cut
*/
int
(i_setcolors)(i_img *im, int index, const i_color *colors, int count) {
  return i_setcolors(im, index, colors, count);
}

