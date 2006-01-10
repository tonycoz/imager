#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
#ifdef __cplusplus
}
#endif

#include "imext.h"
#include "imperl.h"

DEFINE_IMAGER_CALLBACKS;

int
count_color(i_img *im, i_color *color) {
  int x, y, chan;
  i_color c;
  int count = 0;

  for (x = 0; x < im->xsize; ++x) {
    for (y = 0; y < im->ysize; ++y) {
      i_gpix(im, x, y, &c);
      int match = 1;
      for (chan = 0; chan < im->channels; ++chan) {
        if (c.channel[chan] != color->channel[chan]) {
          match = 0;
          break;
        }
      }
      if (match) ++count;
    }
  }

  return count;
}

MODULE = Imager::CountColor   PACKAGE = Imager::CountColor

PROTOTYPES: ENABLE

int
count_color(im, color)
        Imager::ImgRaw im
        Imager::Color color

BOOT:
        PERL_INITIALIZE_IMAGER_CALLBACKS;
