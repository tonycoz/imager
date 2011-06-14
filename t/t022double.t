#!perl -w
use strict;
use Test::More tests => 88;
BEGIN { use_ok(Imager => qw(:all :handy)) }

use Imager::Test qw(test_image is_image);

-d "testout" or mkdir "testout";

Imager->open_log(log => "testout/t022double.log");

use Imager::Test qw(image_bounds_checks test_colorf_gpix test_colorf_glin mask_tests);

use Imager::Color::Float;

my $im_g = Imager::i_img_double_new(100, 101, 1);

ok(Imager::i_img_getchannels($im_g) == 1, 
   "1 channel image channel count mismatch");
ok(Imager::i_img_getmask($im_g) & 1, "1 channel image bad mask");
ok(Imager::i_img_virtual($im_g) == 0, 
  "1 channel image thinks it is virtual");
my $double_bits = length(pack("d", 1)) * 8;
print "# $double_bits double bits\n";
ok(Imager::i_img_bits($im_g) == $double_bits, 
   "1 channel image has bits != $double_bits");
ok(Imager::i_img_type($im_g) == 0, "1 channel image isn't direct");

my @ginfo = i_img_info($im_g);
ok($ginfo[0] == 100, "1 channel image width incorrect");
ok($ginfo[1] == 101, "1 channel image height incorrect");

undef $im_g;

my $im_rgb = Imager::i_img_double_new(100, 101, 3);

ok(Imager::i_img_getchannels($im_rgb) == 3,
   "3 channel image channel count mismatch");
ok((Imager::i_img_getmask($im_rgb) & 7) == 7, "3 channel image bad mask");
ok(Imager::i_img_bits($im_rgb) == $double_bits,
  "3 channel image has bits != $double_bits");
ok(Imager::i_img_type($im_rgb) == 0, "3 channel image isn't direct");

my $redf = NCF(1, 0, 0);
my $greenf = NCF(0, 1, 0);
my $bluef = NCF(0, 0, 1);

# fill with red
for my $y (0..101) {
  Imager::i_plinf($im_rgb, 0, $y, ($redf) x 100);
}

# basic sanity
test_colorf_gpix($im_rgb, 0,  0,   $redf);
test_colorf_gpix($im_rgb, 99, 0,   $redf);
test_colorf_gpix($im_rgb, 0,  100, $redf);
test_colorf_gpix($im_rgb, 99, 100, $redf);
test_colorf_glin($im_rgb, 0,  0,   [ ($redf) x 100 ], 'sanity glin @0');
test_colorf_glin($im_rgb, 0,  100, [ ($redf) x 100 ], 'sanity glin @100');

Imager::i_plinf($im_rgb, 20, 1, ($greenf) x 60);
test_colorf_glin($im_rgb, 0, 1, 
                 [ ($redf) x 20, ($greenf) x 60, ($redf) x 20 ],
		 'check after write');

# basic OO tests
my $ooimg = Imager->new(xsize=>200, ysize=>201, bits=>'double');
ok($ooimg, "couldn't make double image");
is($ooimg->bits, 'double', "oo didn't give double image");
ok(!$ooimg->is_bilevel, 'not monochrome');

# check that the image is copied correctly
my $oocopy = $ooimg->copy;
is($oocopy->bits, 'double', "oo copy didn't give double image");

ok(!Imager->new(xsize=>0, ysize=>1, bits=>'double'),
    "fail making 0 width image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct message");
ok(!Imager->new(xsize=>1, ysize=>0, bits=>'double'),
    "fail making 0 height image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct message");
ok(!Imager->new(xsize=>-1, ysize=>1, bits=>'double'),
    "fail making -ve width image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct message");
ok(!Imager->new(xsize=>1, ysize=>-1, bits=>'double'),
    "fail making -ve height image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct message");
ok(!Imager->new(xsize=>1, ysize=>1, bits=>'double', channels=>0),
    "fail making 0 channel image");
cmp_ok(Imager->errstr, '=~', qr/channels must be between 1 and 4/,
       "and correct message");
ok(!Imager->new(xsize=>1, ysize=>1, bits=>'double', channels=>5),
    "fail making 5 channel image");
cmp_ok(Imager->errstr, '=~', qr/channels must be between 1 and 4/,
       "and correct message");

{
  # https://rt.cpan.org/Ticket/Display.html?id=8213
  # check for handling of memory allocation of very large images
  # only test this on 32-bit machines - on a 64-bit machine it may
  # result in trying to allocate 4Gb of memory, which is unfriendly at
  # least and may result in running out of memory, causing a different
  # type of exit
  use Config;
  SKIP: 
  {
    $Config{ptrsize} == 4
      or skip "don't want to allocate 4Gb", 8;
    my $uint_range = 256 ** $Config{intsize};
    my $dbl_size = $Config{doublesize} || 8;
    my $dim1 = int(sqrt($uint_range/$dbl_size))+1;
    
    my $im_b = Imager->new(xsize=>$dim1, ysize=>$dim1, channels=>1, bits=>'double');
    is($im_b, undef, "integer overflow check - 1 channel");
    
    $im_b = Imager->new(xisze=>$dim1, ysize=>1, channels=>1, bits=>'double');
    ok($im_b, "but same width ok");
    $im_b = Imager->new(xisze=>1, ysize=>$dim1, channels=>1, bits=>'double');
    ok($im_b, "but same height ok");
    cmp_ok(Imager->errstr, '=~', qr/integer overflow/,
           "check the error message");

    # do a similar test with a 3 channel image, so we're sure we catch
    # the same case where the third dimension causes the overflow
    my $dim3 = int(sqrt($uint_range / 3 / $dbl_size))+1;
    
    $im_b = Imager->new(xsize=>$dim3, ysize=>$dim3, channels=>3, bits=>'double');
    is($im_b, undef, "integer overflow check - 3 channel");
    
    $im_b = Imager->new(xsize=>$dim3, ysize=>1, channels=>3, bits=>'double');
    ok($im_b, "but same width ok");
    $im_b = Imager->new(xsize=>1, ysize=>$dim3, channels=>3, bits=>'double');
    ok($im_b, "but same height ok");

    cmp_ok(Imager->errstr, '=~', qr/integer overflow/,
           "check the error message");
  }
}

{ # check the channel mask function
  
  my $im = Imager->new(xsize => 10, ysize=>10, bits=>'double');

  mask_tests($im);
}

{ # bounds checking
  my $im = Imager->new(xsize => 10, ysize=>10, bits=>'double');
  image_bounds_checks($im);
}


{ # convert to rgb double
  my $im = test_image();
  my $imdb = $im->to_rgb_double;
  print "# check conversion to double\n";
  is($imdb->bits, "double", "check bits");
  is_image($im, $imdb, "check image data matches");
}

{ # empty image handling
  my $im = Imager->new;
  ok($im, "make empty image");
  ok(!$im->to_rgb_double, "convert empty image to double");
  is($im->errstr, "empty input image", "check message");
}

Imager->close_log;

unless ($ENV{IMAGER_KEEP_FILES}) {
  unlink "testout/t022double.log";
}
