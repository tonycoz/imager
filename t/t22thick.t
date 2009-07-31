#!perl -w
use strict;
use Test::More tests => 45;
use Imager;
use Imager::Fill;

use constant PI => atan2(0, -1);

use_ok('Imager::Pen::Thick');
{
  my $im = Imager->new(xsize => 200, ysize => 200);
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
 # ok($pen->draw(image => $im, lines => [ $line ]), "draw it");
  $im->setpixel(x => [ map $_->[0], @pts ], 
		'y' => [ map $_->[1], @pts], 
		color => '#00f');
  #my $line2 = Imager::Polyline->new(0, map @$_, reverse,@pts);
  #my $pen2 = Imager::Pen::Thick->new(thickness => 5, color => 'red');
  #ok($pen2->draw(image => $im, lines => [ $line2 ]), "draw another");
  ok($im->write(file => 'testout/t22one.ppm'), "save it");
}

# septagon points
my @septagon = map
  [
   100 + 50 * cos(PI * 2 / 7 * $_),
   100 + 50 * sin(PI * 2 / 7 * $_),
  ], 0 .. 6;

{
  for my $corner (qw/round cut ptc_30/) {
    my $im = Imager->new(xsize => 200, ysize => 200);
    my $pen1 = Imager::Pen::Thick->new
      (
       thickness => 41, 
       color => '#F00',
       corner => $corner,
      );
    ok($pen1, "making $corner pen1")
      or diag(Imager->errstr);
    my $pen2 = Imager::Pen::Thick->new
      (
       thickness => 31, 
       color => '#0F0',
       corner => $corner,
      );
    ok($pen2, "making $corner pen2")
      or diag(Imager->errstr);
    my $line = Imager::Polyline->new(1, map @$_, @septagon);
    ok($pen1->draw(image => $im, lines => [ $line ]),
       "draw heptagon $corner corners");
    my $line2 = Imager::Polyline->new(1, map @$_, reverse @septagon);
    $line2->dump;
    ok($pen2->draw(image => $im, lines => [ $line2 ]),
       "draw heptagon $corner corners (reverse order)");
    ok($im->write(file => "testout/t22c$corner.ppm"), "save it");
  }
}

{
  my $im = Imager->new(xsize => 200, ysize => 200);
  my $step = PI / 15;
  my $start = PI - $step / 2;
  my $y = 20;
  my $green = 64;
  my $blue = 255;
  for my $corner (qw(round cut ptc_30)) {
    my $red = 64;
    my $x = 10;
    my $angle = $start;
    while ($x < 180) {
      my $pen = Imager::Pen::Thick->new
	(
	 thickness => 10,
	 color => [ $red, $green, $blue, 192 ],
	 corner => $corner,
	);
      my $line = Imager::Polyline->new
	(
	 0,
	 $x, $y+50,
	 $x, $y,
	 $x+sin($angle)*50, $y-cos($angle)*50
	);
      ok($pen->draw(image => $im, lines => [ $line ]),
	 "corner $corner, angle $angle");
      $x += 30;
      $angle -= $step;
      $red += 32;
    }
    $y += 60;
    $green += 40;
  }
  ok($im->write(file => 'testout/t22angles.ppm'), "save angles");
}

{ # failure checks
  my $pen = Imager::Pen::Thick->new
    (
     thickness => 40, 
     color => '#F00',
     corner => 'badvalue',
    );
  ok(!$pen, "shouldn't create pen with unknown corner value");
  is(Imager->errstr, "Imager::Pen::Thick::new: Unknown value for corner",
     "check error message");
}

