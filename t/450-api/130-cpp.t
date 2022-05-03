#!perl -w
#
# this tests the API headers are C++ compatible
use strict;
use Test::More;
eval "require Inline::CPP;";
plan skip_all => "Inline::CPP required for testing C++ compatibility" if $@;

use Cwd 'getcwd';
plan skip_all => "Inline won't work in directories with spaces"
  if getcwd() =~ / /;

plan skip_all => "perl 5.005_04, 5.005_05 too buggy"
  if $] =~ /^5\.005_0[45]$/;

-d "testout" or mkdir "testout";

print STDERR "Inline::CPP version $Inline::CPP::VERSION\n";

require Inline;
Inline->import(with => 'Imager');
Inline->import("FORCE"); # force rebuild
#Inline->import(C => Config => OPTIMIZE => "-g");

Inline->bind(CPP => <<'EOS');
#include <math.h>

int pixel_count(Imager::ImgRaw im) {
  return im->xsize * im->ysize;
}

int count_color(Imager::ImgRaw im, Imager::Color c) {
  int count = 0, x, y, chan;
  i_color read_c;

  for (x = 0; x < im->xsize; ++x) {
    for (y = 0; y < im->ysize; ++y) {
      int match = 1;
      i_gpix(im, x, y, &read_c);
      for (chan = 0; chan < im->channels; ++chan) {
        if (read_c.channel[chan] != c->channel[chan]) {
          match = 0;
          break;
        }
      }
      if (match)
        ++count;
    }
  }

  return count;
}

Imager make_10x10() {
  i_img *im = i_img_8_new(10, 10, 3);
  i_color c;
  c.channel[0] = c.channel[1] = c.channel[2] = 255;
  i_box_filled(im, 0, 0, im->xsize-1, im->ysize-1, &c);

  return im;
}

EOS

my $im = Imager->new(xsize=>50, ysize=>50);
is(pixel_count($im), 2500, "pixel_count");

my $black = Imager::Color->new(0,0,0);
is(count_color($im, $black), 2500, "count_color black on black image");

my $im2 = make_10x10();
my $white = Imager::Color->new(255, 255, 255);
is(count_color($im2, $white), 100, "check new image white count");
ok($im2->box(filled=>1, xmin=>1, ymin=>1, xmax => 8, ymax=>8, color=>$black),
   "try new image");
is(count_color($im2, $black), 64, "check modified black count");
is(count_color($im2, $white), 36, "check modified white count");


done_testing();