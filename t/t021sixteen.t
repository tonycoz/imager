#!perl -w
use strict;
BEGIN { $| = 1; print "1..29\n"; }
my $loaded;
END {print "not ok 1\n" unless $loaded;}
use Imager qw(:all :handy);
#use Data::Dumper;
$loaded = 1;
print "ok 1\n";
init_log("testout/t021sixteen.log", 1);

use Imager::Color::Float;

my $im_g = Imager::i_img_16_new(100, 101, 1);

print Imager::i_img_getchannels($im_g) == 1 
  ? "ok 2\n" : "not ok 2 # 1 channel image channel count mismatch\n";
print Imager::i_img_getmask($im_g) & 1 
  ? "ok 3\n" : "not ok 3 # 1 channel image bad mask\n";
print Imager::i_img_virtual($im_g) 
  ? "not ok 4 # 1 channel image thinks it is virtual\n" : "ok 4\n";
print Imager::i_img_bits($im_g) == 16
  ? "ok 5\n" : "not ok 5 # 1 channel image has bits != 16\n";
print Imager::i_img_type($im_g) == 0 # direct
  ? "ok 6\n" : "not ok 6 # 1 channel image isn't direct\n";

my @ginfo = i_img_info($im_g);
print $ginfo[0] == 100 
  ? "ok 7\n" : "not ok 7 # 1 channel image width incorrect\n";
print $ginfo[1] == 101
  ? "ok 8\n" : "not ok 8 # 1 channel image height incorrect\n";

undef $im_g;

my $im_rgb = Imager::i_img_16_new(100, 101, 3);

print Imager::i_img_getchannels($im_rgb) == 3
  ? "ok 9\n" : "not ok 9 # 3 channel image channel count mismatch\n";
print +(Imager::i_img_getmask($im_rgb) & 7) == 7
  ? "ok 10\n" : "not ok 10 # 3 channel image bad mask\n";
print Imager::i_img_bits($im_rgb) == 16
  ? "ok 11\n" : "not ok 11 # 3 channel image has bits != 16\n";
print Imager::i_img_type($im_rgb) == 0 # direct
  ? "ok 12\n" : "not ok 12 # 3 channel image isn't direct\n";

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
my $oo16img = Imager->new(xsize=>200, ysize=>201, bits=>16)
  or print "not ";
print "ok 28\n";
$oo16img->bits == 16 or print "not ";
print "ok 29\n";


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
