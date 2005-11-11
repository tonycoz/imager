#!perl -w
use strict;
use lib 't';
use Test::More tests => 81;

BEGIN { use_ok(Imager => qw(:all :handy)) }
require "t/testtools.pl";

init_log("testout/t022double.log", 1);

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
test_colorf_glin($im_rgb, 0,  0,   ($redf) x 100);
test_colorf_glin($im_rgb, 0,  100, ($redf) x 100);

Imager::i_plinf($im_rgb, 20, 1, ($greenf) x 60);
test_colorf_glin($im_rgb, 0, 1, 
                 ($redf) x 20, ($greenf) x 60, ($redf) x 20);

# basic OO tests
my $ooimg = Imager->new(xsize=>200, ysize=>201, bits=>'double');
ok($ooimg, "couldn't make double image");
ok($ooimg->bits eq 'double', "oo didn't give double image");

# check that the image is copied correctly
my $oocopy = $ooimg->copy;
ok($oocopy->bits eq 'double', "oo copy didn't give double image");

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
    $Config{intsize} == 4
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
  # we want to check all four of ppix() and plin(), ppix() and plinf()
  # basic test procedure:
  #   first using default/all 1s mask, set to white
  #   make sure we got white
  #   set mask to skip a channel, set to grey
  #   make sure only the right channels set
  
  print "# channel mask tests\n";
  my $im = Imager->new(xsize => 10, ysize=>10, bits=>'double');

  # 8-bit color tests
  my $white = NC(255, 255, 255);
  my $grey = NC(128, 128, 128);
  my $white_grey = NC(128, 255, 128);

  print "# with ppix\n";
  ok($im->setmask(mask=>~0), "set to default mask");
  ok($im->setpixel(x=>0, 'y'=>0, color=>$white), "set to white all channels");
  test_color_gpix($im->{IMG}, 0, 0, $white);
  ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  ok($im->setpixel(x=>0, 'y'=>0, color=>$grey), "set to grey, no channel 2");
  test_color_gpix($im->{IMG}, 0, 0, $white_grey);

  print "# with plin\n";
  ok($im->setmask(mask=>~0), "set to default mask");
  ok($im->setscanline(x=>0, 'y'=>1, pixels => [$white]), 
     "set to white all channels");
  test_color_gpix($im->{IMG}, 0, 1, $white);
  ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  ok($im->setscanline(x=>0, 'y'=>1, pixels=>[$grey]), 
     "set to grey, no channel 2");
  test_color_gpix($im->{IMG}, 0, 1, $white_grey);

  # float color tests
  my $whitef = NCF(1.0, 1.0, 1.0);
  my $greyf = NCF(0.5, 0.5, 0.5);
  my $white_greyf = NCF(0.5, 1.0, 0.5);

  print "# with ppixf\n";
  ok($im->setmask(mask=>~0), "set to default mask");
  ok($im->setpixel(x=>0, 'y'=>2, color=>$whitef), "set to white all channels");
  test_colorf_gpix($im->{IMG}, 0, 2, $whitef);
  ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  ok($im->setpixel(x=>0, 'y'=>2, color=>$greyf), "set to grey, no channel 2");
  test_colorf_gpix($im->{IMG}, 0, 2, $white_greyf);

  print "# with plinf\n";
  ok($im->setmask(mask=>~0), "set to default mask");
  ok($im->setscanline(x=>0, 'y'=>3, pixels => [$whitef]), 
     "set to white all channels");
  test_colorf_gpix($im->{IMG}, 0, 3, $whitef);
  ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  ok($im->setscanline(x=>0, 'y'=>3, pixels=>[$greyf]), 
     "set to grey, no channel 2");
  test_colorf_gpix($im->{IMG}, 0, 3, $white_greyf);
}

sub NCF {
  return Imager::Color::Float->new(@_);
}

sub test_colorf_gpix {
  my ($im, $x, $y, $expected) = @_;
  my $c = Imager::i_gpixf($im, $x, $y);
  ok($c, "got gpix ($x, $y)");
  unless (ok(colorf_cmp($c, $expected) == 0,
	     "got right color ($x, $y)")) {
    print "# got: (", join(",", ($c->rgba)[0,1,2]), ")\n";
    print "# expected: (", join(",", ($expected->rgba)[0,1,2]), ")\n";
  }
}

sub test_color_gpix {
  my ($im, $x, $y, $expected) = @_;
  my $c = Imager::i_get_pixel($im, $x, $y);
  ok($c, "got gpix ($x, $y)");
  unless (ok(color_cmp($c, $expected) == 0,
     "got right color ($x, $y)")) {
    print "# got: (", join(",", ($c->rgba)[0,1,2]), ")\n";
    print "# expected: (", join(",", ($expected->rgba)[0,1,2]), ")\n";
  }
}

sub test_colorf_glin {
  my ($im, $x, $y, @pels) = @_;

  my @got = Imager::i_glinf($im, $x, $x+@pels, $y);
  is(@got, @pels, "check number of pixels ($x, $y)");
  ok(!grep(colorf_cmp($pels[$_], $got[$_]), 0..$#got),
     "check colors ($x, $y)");
}

sub colorf_cmp {
  my ($c1, $c2) = @_;
  my @s1 = map { int($_*65535.99) } $c1->rgba;
  my @s2 = map { int($_*65535.99) } $c2->rgba;

  # print "# (",join(",", @s1[0..2]),") <=> (",join(",", @s2[0..2]),")\n";
  return $s1[0] <=> $s2[0] 
    || $s1[1] <=> $s2[1]
      || $s1[2] <=> $s2[2];
}

sub color_cmp {
  my ($c1, $c2) = @_;

  my @s1 = $c1->rgba;
  my @s2 = $c2->rgba;

  return $s1[0] <=> $s2[0] 
    || $s1[1] <=> $s2[1]
      || $s1[2] <=> $s2[2];
}
