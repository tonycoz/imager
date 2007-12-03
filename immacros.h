/* 
   Imager "functions" implemented as macros 

   I suppose these could go in imdatatypes, but they aren't types.
*/
#ifndef IMAGER_IMMACROS_H_
#define IMAGER_IMMACROS_H_

/*
=item i_img_has_alpha(im)

=category Image Information

Return true if the image has an alpha channel.

=cut
*/

#define i_img_has_alpha(im) ((im)->channels == 2 || (im)->channels == 4)

/*
=item i_img_color_channels(im)

=category Image Information

The number of channels holding color information.

=cut
*/

#define i_img_color_channels(im) (i_img_has_alpha(im) ? (im)->channels - 1 : (im)->channels)

#endif
