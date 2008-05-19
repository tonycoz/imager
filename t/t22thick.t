#!perl -w
use strict;
use Test::More tests => 3;
use Imager;

use_ok('Imager::Pen::Thick');
my $im = Imager->new(xsize => 100, ysize => 100);
my $pen = Imager::Pen::Thick->new(thickness => 5, color => '#FFF');

my $line = Imager::Polyline->new(0, 10, 10, 90, 10, 90, 90, 10, 90);
ok($line, "made a polyline object");
ok($pen->draw(image => $im, lines => [ $line ]), "draw it");
#$im->setpixel(x => [ 10, 90, 90 ], y => [10,10,90], color => '#00f');
ok($im->write(file => 'testout/t22one.ppm'), "save it");

