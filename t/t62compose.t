#!perl -w
use strict;
use Imager qw(:handy);
use Test::More tests => 4;
use Imager::Test qw(is_image);

{
  my $blue = NC(0, 0, 255);
  my $red =  NC(255, 0, 0);
  my $green2 = NC(0, 255, 0, 128);
  my $green2_on_blue = NC(0, 128, 127);
  my $red3_on_blue = NC(85, 0, 170);
  my $green6_on_blue = NC(42, 0, 203);

  my $targ = Imager->new(xsize => 100, ysize => 100);
  $targ->box(color => $blue, filled => 1);
  is($targ->type, "direct", "check target image type");
  is($targ->bits, 8, "check target bits");

  my $src = Imager->new(xsize => 40, ysize => 40, channels => 4);
  $src->box(filled => 1, color => $red, xmax => 19, ymax => 19);
  $src->box(filled => 1, xmin => 20, color => $green2);

  # no mask
  {
    my $work = $targ->copy;
    ok($work->compose(src => $src, tx => 5, ty => 10),
       "simple compose");
    my $cmp = $targ->copy;
    $cmp->box(filled => 1, color => $red,
	      xmin=> 5, ymin => 10, xmax => 24, ymax => 29);
    $cmp->box(filled => 1, color => $green2_on_blue,
	      xmin => 25, ymin => 10, xmax => 44, ymax => 49);
    is_image($work, $cmp, "check match");
  }
  { # compose at 1/3
    my $work = $targ->copy;
    ok($work->compose(src => $src, tx => 7, ty => 33, opacity => 1/3),
       "simple compose at 1/3");
    my $cmp = $targ->copy;
    $cmp->box(filled => 1, color => $red3_on_blue,
	      xmin => 7, ymin => 33, xmax => 26, ymax => 52);
    $cmp->box
  }
}
