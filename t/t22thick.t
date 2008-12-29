#!perl -w
use strict;
use Test::More tests => 3;
use Imager;
use Imager::Fill;

use_ok('Imager::Pen::Thick');
my $im = Imager->new(xsize => 200, ysize => 200);
{
  #my $pen = Imager::Pen::Thick->new(thickness => 5, color => '#888');
  my $pen = Imager::Pen::Thick->new
    (
     thickness => 17, 
     fill => Imager::Fill->new(solid => '#880', combine => 'normal'), 
     front => 'round', 
     back => 'round',
     corner => 'round',
    );
  
  my @pts = ( [ 20, 20 ], [ 180, 20 ], [ 20, 180 ], [ 180, 180 ]);#, [ 10, 90 ]);
  my $line = Imager::Polyline->new(0, map @$_, @pts);
  ok($line, "made a polyline object");
  ok($pen->draw(image => $im, lines => [ $line ]), "draw it");
  $im->setpixel(x => [ map $_->[0], @pts ], 
		'y' => [ map $_->[1], @pts], 
		color => '#00f');
  #my $line2 = Imager::Polyline->new(0, 20, 20, 80, 80);
  #my $pen2 = Imager::Pen::Thick->new(thickness => 5, color => 'red');
  #ok($pen2->draw(image => $im, lines => [ $line2 ]), "draw another");
}
ok($im->write(file => 'testout/t22one.ppm'), "save it");

