#!perl -w
use strict;
use Test::More tests => 34;
use Imager qw(:all :handy);
use Imager::Test qw(is_color3);
init_log("testout/t020masked.log", 1);

my $base_rgb = Imager::ImgRaw::new(100, 100, 3);
# put something in there
my $red = NC(255, 0, 0);
my $green = NC(0, 255, 0);
my $blue = NC(0, 0, 255);
my $white = NC(255, 255, 255);
my @cols = ($red, $green, $blue);
for my $y (0..99) {
  Imager::i_plin($base_rgb, 0, $y, ($cols[$y % 3] ) x 100);
}

# first a simple subset image
my $s_rgb = Imager::i_img_masked_new($base_rgb, undef, 25, 25, 50, 50);

is(Imager::i_img_getchannels($s_rgb), 3,
   "1 channel image channel count match");
ok(Imager::i_img_getmask($s_rgb) & 1,
   "1 channel image mask");
ok(Imager::i_img_virtual($s_rgb),
   "1 channel image thinks it isn't virtual");
is(Imager::i_img_bits($s_rgb), 8,
   "1 channel image has bits == 8");
is(Imager::i_img_type($s_rgb), 0, # direct
   "1 channel image is direct");

my @ginfo = i_img_info($s_rgb);
is($ginfo[0], 50, "check width");
is($ginfo[1], 50, "check height");

# sample some pixels through the subset
my $c = Imager::i_get_pixel($s_rgb, 0, 0);
is_color3($c, 0, 255, 0, "check (0,0)");
$c = Imager::i_get_pixel($s_rgb, 49, 49);
# (25+49)%3 = 2
is_color3($c, 0, 0, 255, "check (49,49)");

# try writing to it
for my $y (0..49) {
  Imager::i_plin($s_rgb, 0, $y, ($cols[$y % 3]) x 50);
}
pass("managed to write to it");
# and checking the target image
$c = Imager::i_get_pixel($base_rgb, 25, 25);
is_color3($c, 255, 0, 0, "check (25,25)");
$c = Imager::i_get_pixel($base_rgb, 29, 29);
is_color3($c, 0, 255, 0, "check (29,29)");

undef $s_rgb;

# a basic background
for my $y (0..99) {
  Imager::i_plin($base_rgb, 0, $y, ($red ) x 100);
}
my $mask = Imager::ImgRaw::new(50, 50, 1);
# some venetian blinds
for my $y (4..20) {
  Imager::i_plin($mask, 5, $y*2, ($white) x 40);
}
# with a strip down the middle
for my $y (0..49) {
  Imager::i_plin($mask, 20, $y, ($white) x 8);
}
my $m_rgb = Imager::i_img_masked_new($base_rgb, $mask, 25, 25, 50, 50);
ok($m_rgb, "make masked with mask");
for my $y (0..49) {
  Imager::i_plin($m_rgb, 0, $y, ($green) x 50);
}
my @color_tests =
  (
   [ 25+0,  25+0,  $red ],
   [ 25+19, 25+0,  $red ],
   [ 25+20, 25+0,  $green ],
   [ 25+27, 25+0,  $green ],
   [ 25+28, 25+0,  $red ],
   [ 25+49, 25+0,  $red ],
   [ 25+19, 25+7,  $red ],
   [ 25+19, 25+8,  $green ],
   [ 25+19, 25+9,  $red ],
   [ 25+0,  25+8,  $red ],
   [ 25+4,  25+8,  $red ],
   [ 25+5,  25+8,  $green ],
   [ 25+44, 25+8,  $green ],
   [ 25+45, 25+8,  $red ],
   [ 25+49, 25+49, $red ],
  );
my $test_num = 15;
for my $test (@color_tests) {
  my ($x, $y, $testc) = @$test;
  my ($r, $g, $b) = $testc->rgba;
  my $c = Imager::i_get_pixel($base_rgb, $x, $y);
  is_color3($c, $r, $g, $b, "at ($x, $y)");
}

{
  # tests for the OO versions, fairly simple, since the basic functionality
  # is covered by the low-level interface tests
   
  my $base = Imager->new(xsize=>100, ysize=>100);
  ok($base, "make base OO image");
  $base->box(color=>$blue, filled=>1); # fill it all
  my $mask = Imager->new(xsize=>80, ysize=>80, channels=>1);
  $mask->box(color=>$white, filled=>1, xmin=>5, xmax=>75, ymin=>5, ymax=>75);
  my $m_img = $base->masked(mask=>$mask, left=>5, top=>5);
  ok($m_img, "make masked OO image");
  is($m_img->getwidth, 80, "check width");
  $m_img->box(color=>$green, filled=>1);
  my $c = $m_img->getpixel(x=>0, y=>0);
  is_color3($c, 0, 0, 255, "check (0,0)");
  $c = $m_img->getpixel(x => 5, y => 5);
  is_color3($c, 0, 255, 0, "check (5,5)");

  # older versions destroyed the Imager::ImgRaw object manually in 
  # Imager::DESTROY rather than letting Imager::ImgRaw::DESTROY 
  # destroy the object
  # so we test here by destroying the base and mask objects and trying 
  # to draw to the masked wrapper
  # you may need to test with ElectricFence to trigger the problem
  undef $mask;
  undef $base;
  $m_img->box(color=>$blue, filled=>1);
  pass("didn't crash unreffing base or mask for masked image");
}
