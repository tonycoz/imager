#!perl -w
use strict;
use Test::More tests => 85;

BEGIN { use_ok(Imager=>qw(:all :handy)) }

init_log("testout/t021sixteen.log", 1);

require "t/testtools.pl";

use Imager::Color::Float;

my $im_g = Imager::i_img_16_new(100, 101, 1);

is(Imager::i_img_getchannels($im_g), 1, "1 channel image channel count");
ok(Imager::i_img_getmask($im_g) & 1, "1 channel image mask");
ok(!Imager::i_img_virtual($im_g), "shouldn't be marked virtual");
is(Imager::i_img_bits($im_g), 16, "1 channel image has bits == 16");
is(Imager::i_img_type($im_g), 0, "1 channel image isn't direct");

my @ginfo = i_img_info($im_g);
is($ginfo[0], 100, "1 channel image width");
is($ginfo[1], 101, "1 channel image height");

undef $im_g;

my $im_rgb = Imager::i_img_16_new(100, 101, 3);

is(Imager::i_img_getchannels($im_rgb), 3, "3 channel image channel count");
ok((Imager::i_img_getmask($im_rgb) & 7) == 7, "3 channel image mask");
is(Imager::i_img_bits($im_rgb), 16, "3 channel image bits");
is(Imager::i_img_type($im_rgb), 0, "3 channel image type");

my $redf = NCF(1, 0, 0);
my $greenf = NCF(0, 1, 0);
my $bluef = NCF(0, 0, 1);

# fill with red
for my $y (0..101) {
  Imager::i_plinf($im_rgb, 0, $y, ($redf) x 100);
}
pass("fill with red");
# basic sanity
test_colorf_gpix($im_rgb, 0,  0,   $redf);
test_colorf_gpix($im_rgb, 99, 0,   $redf);
test_colorf_gpix($im_rgb, 0,  100, $redf);
test_colorf_gpix($im_rgb, 99, 100, $redf);
test_colorf_glin($im_rgb, 0,  0,   ($redf) x 100);
test_colorf_glin($im_rgb, 0,  100, ($redf) x 100);

Imager::i_plinf($im_rgb, 20, 1, ($greenf) x 60);
test_colorf_glin($im_rgb, 0, 1, 
                 ($redf) x 20, ($greenf) x 60, ($redf) x 20);

# basic OO tests
my $oo16img = Imager->new(xsize=>200, ysize=>201, bits=>16);
ok($oo16img, "make a 16-bit oo image");
is($oo16img->bits,  16, "test bits");

# make sure of error handling
ok(!Imager->new(xsize=>0, ysize=>1, bits=>16),
    "fail to create a 0 pixel wide image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct error message");

ok(!Imager->new(xsize=>1, ysize=>0, bits=>16),
    "fail to create a 0 pixel high image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct error message");

ok(!Imager->new(xsize=>-1, ysize=>1, bits=>16),
    "fail to create a negative width image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct error message");

ok(!Imager->new(xsize=>1, ysize=>-1, bits=>16),
    "fail to create a negative height image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct error message");

ok(!Imager->new(xsize=>-1, ysize=>-1, bits=>16),
    "fail to create a negative width/height image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct error message");

ok(!Imager->new(xsize=>1, ysize=>1, bits=>16, channels=>0),
    "fail to create a zero channel image");
cmp_ok(Imager->errstr, '=~', qr/channels must be between 1 and 4/,
       "and correct error message");
ok(!Imager->new(xsize=>1, ysize=>1, bits=>16, channels=>5),
    "fail to create a five channel image");
cmp_ok(Imager->errstr, '=~', qr/channels must be between 1 and 4/,
       "and correct error message");

{
  # https://rt.cpan.org/Ticket/Display.html?id=8213
  # check for handling of memory allocation of very large images
  # only test this on 32-bit machines - on a 64-bit machine it may
  # result in trying to allocate 4Gb of memory, which is unfriendly at
  # least and may result in running out of memory, causing a different
  # type of exit
 SKIP: {
    use Config;
    $Config{intsize} == 4
      or skip("don't want to allocate 4Gb", 8);
    my $uint_range = 256 ** $Config{intsize};
    print "# range $uint_range\n";
    my $dim1 = int(sqrt($uint_range/2))+1;
    
    my $im_b = Imager->new(xsize=>$dim1, ysize=>$dim1, channels=>1, bits=>16);
    is($im_b, undef, "integer overflow check - 1 channel");
    
    $im_b = Imager->new(xisze=>$dim1, ysize=>1, channels=>1, bits=>16);
    ok($im_b, "but same width ok");
    $im_b = Imager->new(xisze=>1, ysize=>$dim1, channels=>1, bits=>16);
    ok($im_b, "but same height ok");
    cmp_ok(Imager->errstr, '=~', qr/integer overflow/,
           "check the error message");

    # do a similar test with a 3 channel image, so we're sure we catch
    # the same case where the third dimension causes the overflow
    my $dim3 = int(sqrt($uint_range / 3 / 2))+1;
    
    $im_b = Imager->new(xsize=>$dim3, ysize=>$dim3, channels=>3, bits=>16);
    is($im_b, undef, "integer overflow check - 3 channel");
    
    $im_b = Imager->new(xisze=>$dim3, ysize=>1, channels=>3, bits=>16);
    ok($im_b, "but same width ok");
    $im_b = Imager->new(xisze=>1, ysize=>$dim3, channels=>3, bits=>16);
    ok($im_b, "but same height ok");

    cmp_ok(Imager->errstr, '=~', qr/integer overflow/,
           "check the error message");

    # check we can allocate a scanline, unlike double images the scanline
    # in the image itself is smaller than a line of i_fcolor
    # divide by 2 to get to int range, by 2 for 2 bytes/pixel, by 3 to 
    # fit the image allocation in, but for the floats to overflow
    my $dim4 = $uint_range / 2 / 2 / 3;
    my $im_o = Imager->new(xsize=>$dim4, ysize=>1, channels=>1, bits=>16);
    is($im_o, undef, "integer overflow check - scanline");
    cmp_ok(Imager->errstr, '=~',
           qr/integer overflow calculating scanline allocation/,
           "check error message");
  }
}

{ # check the channel mask function
  
  my $im = Imager->new(xsize => 10, ysize=>10, bits=>16);

  mask_tests($im, 1.0/65535);
}
