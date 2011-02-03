#!perl -w
use strict;
use Imager;
use Imager::Test qw(is_image);
use Test::More;

$Imager::formats{"tiff"}
  or plan skip_all => "no tiff support";

-d "testout" or mkdir "testout";

plan tests => 2;

my $dest = Imager->new(xsize => 100, ysize => 100, channels => 4);
$dest->box(filled => 1, color => '0000FF');
my $src = Imager->new(xsize => 100, ysize => 100, channels => 4);
$src->circle(color => 'FF0000', x => 50, y => 60, r => 40, aa => 1);
ok($dest->rubthrough(src => $src, src_minx => 10, src_miny => 20, src_maxx => 90,
	       tx => 10, ty => 10), "rubthrough");
ok($dest->write(file => "testout/x11rubthru.tif"), "save it");

