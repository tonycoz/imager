#!perl -w
use strict;
use Test::More tests => 3;
use Imager;
use Imager::Fill;

use_ok('Imager::Pen::Thick');
my $im = Imager->new(xsize => 100, ysize => 100);
{
  #my $pen = Imager::Pen::Thick->new(thickness => 5, color => '#888');
  my $pen = Imager::Pen::Thick->new
    (
     thickness => 9, 
     fill => Imager::Fill->new(solid => '#888', combine => 'normal'), 
     front => 'round', 
     back => 'round'
    );
  
  my $line = Imager::Polyline->new(0, 10, 10, 90, 10, 10, 90, 90, 90);#, 10, 90);
  ok($line, "made a polyline object");
  ok($pen->draw(image => $im, lines => [ $line ]), "draw it");
  $im->setpixel(x => [ 10, 90, 90 ], 'y' => [10,10,90], color => '#00f');
  #my $line2 = Imager::Polyline->new(0, 20, 20, 80, 80);
  #my $pen2 = Imager::Pen::Thick->new(thickness => 5, color => 'red');
  #ok($pen2->draw(image => $im, lines => [ $line2 ]), "draw another");
}
ok($im->write(file => 'testout/t22one.ppm'), "save it");

