#!perl -w
use strict;
BEGIN { $| = 1; print "1..50\n"; }
my $loaded;
END {print "not ok 1\n" unless $loaded;}
use Imager qw(:all :handy);
#use Data::Dumper;
$loaded = 1;
print "ok 1\n";

require "t/testtools.pl";

init_log("testout/t022double.log", 1);

use Imager::Color::Float;

my $im_g = Imager::i_img_double_new(100, 101, 1);

ok(2, Imager::i_img_getchannels($im_g) == 1, 
   "1 channel image channel count mismatch");
ok(3, Imager::i_img_getmask($im_g) & 1, "1 channel image bad mask");
ok(4, Imager::i_img_virtual($im_g) == 0, 
  "1 channel image thinks it is virtual");
my $double_bits = length(pack("d", 1)) * 8;
print "# $double_bits double bits\n";
ok(5, Imager::i_img_bits($im_g) == $double_bits, 
   "1 channel image has bits != $double_bits");
ok(6, Imager::i_img_type($im_g) == 0, "1 channel image isn't direct");

my @ginfo = i_img_info($im_g);
ok(7, $ginfo[0] == 100, "1 channel image width incorrect");
ok(8, $ginfo[1] == 101, "1 channel image height incorrect");

undef $im_g;

my $im_rgb = Imager::i_img_double_new(100, 101, 3);

ok(9, Imager::i_img_getchannels($im_rgb) == 3,
   "3 channel image channel count mismatch");
ok(10, (Imager::i_img_getmask($im_rgb) & 7) == 7, "3 channel image bad mask");
ok(11, Imager::i_img_bits($im_rgb) == $double_bits,
  "3 channel image has bits != $double_bits");
ok(12, Imager::i_img_type($im_rgb) == 0, "3 channel image isn't direct");

my $redf = NCF(1, 0, 0);
my $greenf = NCF(0, 1, 0);
my $bluef = NCF(0, 0, 1);

# fill with red
for my $y (0..101) {
  Imager::i_plinf($im_rgb, 0, $y, ($redf) x 100);
}
print "ok 13\n";
# basic sanity
test_colorf_gpix(14, $im_rgb, 0,  0,   $redf);
test_colorf_gpix(16, $im_rgb, 99, 0,   $redf);
test_colorf_gpix(18, $im_rgb, 0,  100, $redf);
test_colorf_gpix(20, $im_rgb, 99, 100, $redf);
test_colorf_glin(22, $im_rgb, 0,  0,   ($redf) x 100);
test_colorf_glin(24, $im_rgb, 0,  100, ($redf) x 100);

Imager::i_plinf($im_rgb, 20, 1, ($greenf) x 60);
test_colorf_glin(26, $im_rgb, 0, 1, 
                 ($redf) x 20, ($greenf) x 60, ($redf) x 20);

# basic OO tests
my $ooimg = Imager->new(xsize=>200, ysize=>201, bits=>'double');
ok(28, $ooimg, "couldn't make double image");
ok(29, $ooimg->bits eq 'double', "oo didn't give double image");

# check that the image is copied correctly
my $oocopy = $ooimg->copy;
ok(30, $oocopy->bits eq 'double', "oo copy didn't give double image");

my $num = 31;
okn($num++, !Imager->new(xsize=>0, ysize=>1, bits=>'double'),
    "fail making 0 width image");
matchn($num++, Imager->errstr, qr/Image sizes must be positive/,
       "and correct message");
okn($num++, !Imager->new(xsize=>1, ysize=>0, bits=>'double'),
    "fail making 0 height image");
matchn($num++, Imager->errstr, qr/Image sizes must be positive/,
       "and correct message");
okn($num++, !Imager->new(xsize=>-1, ysize=>1, bits=>'double'),
    "fail making -ve width image");
matchn($num++, Imager->errstr, qr/Image sizes must be positive/,
       "and correct message");
okn($num++, !Imager->new(xsize=>1, ysize=>-1, bits=>'double'),
    "fail making -ve height image");
matchn($num++, Imager->errstr, qr/Image sizes must be positive/,
       "and correct message");
okn($num++, !Imager->new(xsize=>1, ysize=>1, bits=>'double', channels=>0),
    "fail making 0 channel image");
matchn($num++, Imager->errstr, qr/channels must be between 1 and 4/,
       "and correct message");
okn($num++, !Imager->new(xsize=>1, ysize=>1, bits=>'double', channels=>5),
    "fail making 5 channel image");
matchn($num++, Imager->errstr, qr/channels must be between 1 and 4/,
       "and correct message");

{
  # https://rt.cpan.org/Ticket/Display.html?id=8213
  # check for handling of memory allocation of very large images
  # only test this on 32-bit machines - on a 64-bit machine it may
  # result in trying to allocate 4Gb of memory, which is unfriendly at
  # least and may result in running out of memory, causing a different
  # type of exit
  use Config;
  if ($Config{intsize} == 4) {
    my $uint_range = 256 ** $Config{intsize};
    my $dbl_size = $Config{doublesize} || 8;
    my $dim1 = int(sqrt($uint_range/$dbl_size))+1;
    
    my $im_b = Imager->new(xsize=>$dim1, ysize=>$dim1, channels=>1, bits=>'double');
    isn($num++, $im_b, undef, "integer overflow check - 1 channel");
    
    $im_b = Imager->new(xisze=>$dim1, ysize=>1, channels=>1, bits=>'double');
    okn($num++, $im_b, "but same width ok");
    $im_b = Imager->new(xisze=>1, ysize=>$dim1, channels=>1, bits=>'double');
    okn($num++, $im_b, "but same height ok");
    matchn($num++, Imager->errstr, qr/integer overflow/,
           "check the error message");

    # do a similar test with a 3 channel image, so we're sure we catch
    # the same case where the third dimension causes the overflow
    my $dim3 = int(sqrt($uint_range / 3 / $dbl_size))+1;
    
    $im_b = Imager->new(xsize=>$dim3, ysize=>$dim3, channels=>3, bits=>'double');
    isn($num++, $im_b, undef, "integer overflow check - 3 channel");
    
    $im_b = Imager->new(xisze=>$dim3, ysize=>1, channels=>3, bits=>'double');
    okn($num++, $im_b, "but same width ok");
    $im_b = Imager->new(xisze=>1, ysize=>$dim3, channels=>3, bits=>'double');
    okn($num++, $im_b, "but same height ok");

    matchn($num++, Imager->errstr, qr/integer overflow/,
           "check the error message");
  }
  else {
    skipn($num, 8, "don't want to allocate 4Gb");
    $num += 8;
  }
}

sub NCF {
  return Imager::Color::Float->new(@_);
}

sub test_colorf_gpix {
  my ($test_base, $im, $x, $y, $expected) = @_;
  my $c = Imager::i_gpixf($im, $x, $y);
  $c or print "not ";
  print "ok ",$test_base++,"\n";
  colorf_cmp($c, $expected) == 0 or print "not ";
  print "ok ",$test_base++,"\n";
}

sub test_colorf_glin {
  my ($test_base, $im, $x, $y, @pels) = @_;

  my @got = Imager::i_glinf($im, $x, $x+@pels, $y);
  @got == @pels or print "not ";
  print "ok ",$test_base++,"\n";
  grep(colorf_cmp($pels[$_], $got[$_]), 0..$#got) and print "not ";
  print "ok ",$test_base++,"\n";
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

sub ok {
  my ($test_num, $ok, $comment) = @_;

  if ($ok) {
    print "ok $test_num\n";
  }
  else {
    print "not ok $test_num # $comment\n";
  }
}
