#!perl -w

use strict;
use Test::More tests => 12;

use Imager qw/NC/;
use Imager::Test qw(is_image is_color3);

sub PI () { 3.14159265358979323846 }

-d "testout" or mkdir "testout";

my @cleanup;
push @cleanup, "testout/060-polypoly.log";
Imager->open_log(log => "testout/060-polypoly.log");

my $red   = NC(255,0,0);
my $green = NC(0,255,0);
my $blue  = NC(0,0,255);
my $white = NC(255,255,255);
my $black = NC(0, 0, 0);

END {
  unlink @cleanup unless $ENV{IMAGER_KEEP_FILES};
  rmdir "testout";
}

{
  my $out = "testout/060-ppsimple.ppm";
  my $im = Imager->new(xsize => 100, ysize => 100);
  ok($im->polypolygon
     (
      filled => 1,
      color => $red,
      points =>
      [
       [
	[ 20, 20, 40, 40 ],
	[ 20, 80, 80, 20 ],
       ],
       [
	[ 60, 60, 80, 80 ],
	[ 20, 80, 80, 20 ],
       ],
      ]
     ), "simple filled polypolygon");
  ok($im->write(file => $out), "save to $out");
  my $cmp = Imager->new(xsize => 100, ysize => 100);
  $cmp->box(filled => 1, color => $red, box => [ 20, 20, 39, 79 ]);
  $cmp->box(filled => 1, color => $red, box => [ 60, 20, 79, 79 ]);
  is_image($im, $cmp, "check expected output");
  push @cleanup, $out;
}

{
  my $im = Imager->new(xsize => 100, ysize => 100);
  my $cross_cw =
    [
     [
      [ 10, 90, 90, 10 ],
      [ 40, 40, 60, 60 ],
     ],
     [
      [ 40, 60, 60, 40 ],
      [ 10, 10, 90, 90 ],
     ],
    ];
  ok($im->polypolygon
     (
      filled => 1,
      color => $red,
      points =>$cross_cw,
      mode => "nonzero",
     ), "cross polypolygon nz");
  save($im, "060-ppcrossnz.ppm");
  my $cmp = Imager->new(xsize => 100, ysize => 100);
  $cmp->box(filled => 1, color => $red, box => [ 10, 40, 89, 59 ]);
  $cmp->box(filled => 1, color => $red, box => [ 40, 10, 59, 89 ]);
  is_image($im, $cmp, "check expected output");

  my $im2 = Imager->new(xsize => 100, ysize => 100);
  ok($im2->polypolygon
     (
      filled => 1,
      color => $red,
      points =>$cross_cw,
      #mode => "nonzero", # default to evenodd
     ), "cross polypolygon eo");
  save($im2, "060-ppcrosseo.ppm");

  $cmp->box(filled => 1, color => $black, box => [ 40, 40, 59, 59 ]);
  is_image($im2, $cmp, "check expected output");

  # same as cross_cw except that the second box is in reversed order
  my $cross_diff =
    [
     [
      [ 10, 90, 90, 10 ],
      [ 40, 40, 60, 60 ],
     ],
     [
      [ 40, 60, 60, 40 ],
      [ 90, 90, 10, 10 ],
     ],
    ];
  my $im3 = Imager->new(xsize => 100, ysize => 100);
  ok($im3->polypolygon
     (
      filled => 1,
      color => $red,
      points => $cross_diff,
      mode => "nonzero",
     ), "cross polypolygon diff");
  is_image($im3, $cmp, "check expected output");
  save($im3, "060-ppcrossdiff.ppm");
}

Imager->close_log;

sub save {
  my ($im, $file) = @_;

  $file = "testout/" . $file;
  push @cleanup, $file;
  ok($im->write(file => $file), "save to $file");
}
