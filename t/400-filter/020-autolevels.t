#!perl -w
use strict;
use Imager;
use Test::More tests => 4;
use Imager::Test qw(is_image);

-d "testout" or mkdir "testout";

Imager->open_log(log => "testout/filters-autolev.log");

my $base = Imager->new(xsize => 10, ysize => 10);
$base->box(filled => 1, xmax => 4, color => "404040");
$base->box(filled => 1, xmin => 5, color => "C0C0C0");

{
  my $cmp = Imager->new(xsize => 10, ysize => 10);
  $cmp->box(filled => 1, xmax => 4, color => "#000");
  $cmp->box(filled => 1, xmin => 5, color => "#FFF");

  {
    my $work = $base->copy;
    #$work->write(file => "testout/autolevel-base.ppm");
    ok($work->filter(type => "autolevels"), "default autolevels");
    #$work->write(file => "testout/autolevel-filtered.ppm");
    #$cmp->write(file => "testout/autolevel-cmp.ppm");
    is_image($work, $cmp, "check we got expected image");
  }

  {
    my $work = $base->to_rgb_double;
    ok($work->filter(type => "autolevels"), "default autolevels (double)");
    is_image($work, $cmp, "check we got expected image");
    $work->write(file => "testout/autolevel-filtered.ppm", pnm_write_wide_data => 1);
  }
}


Imager->close_log;
