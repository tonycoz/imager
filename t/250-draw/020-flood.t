#!perl -w
use strict;
use Test::More tests => 113;
use Imager;
use Imager::Test qw(is_image);

-d "testout" or mkdir "testout";

{ # flood_fill wouldn't fill to the right if the area was just a
  # single scan-line
  my $im = Imager->new(xsize => 5, ysize => 3);
  ok($im, "make flood_fill test image");
  ok($im->line(x1 => 0, y1 => 1, x2 => 4, y2 => 1, color => "white"),
     "create fill area");
  ok($im->flood_fill(x => 3, y => 1, color => "blue"),
     "fill it");
  my $cmp = Imager->new(xsize => 5, ysize => 3);
  ok($cmp, "make test image");
  ok($cmp->line(x1 => 0, y1 => 1, x2 => 4, y2 => 1, color => "blue"),
     "synthezied filled area");
  is_image($im, $cmp, "flood_fill filled horizontal line");
}

SKIP:
{ # flood_fill won't fill entire line below if line above is shorter
  my $im = Imager->new(file => "testimg/filltest.ppm");
  ok($im, "Load test image")
    or skip("Couldn't load test image: " . Imager->errstr, 3);

  # fill from first bad place
  my $fill1 = $im->copy;
  ok($fill1->flood_fill(x => 8, y => 2, color => "#000000"),
     "fill from a top most spot");
  my $cmp = Imager->new(xsize => $im->getwidth, ysize => $im->getheight);
  is_image($fill1, $cmp, "check it filled the lot");
  ok($fill1->write(file => "testout/t22fill1.ppm"), "save");

  # second bad place
  my $fill2 = $im->copy;
  ok($fill2->flood_fill(x => 17, y => 3, color => "#000000"),
     "fill from not quite top most spot");
  is_image($fill2, $cmp, "check it filled the lot");
  ok($fill2->write(file => "testout/t22fill2.ppm"), "save");
}

{ # verticals
  my $im = vimage("FFFFFF");
  my $cmp = vimage("FF0000");

  ok($im->flood_fill(x => 4, y=> 8, color => "FF0000"),
     "fill at bottom of vertical well");
  is_image($im, $cmp, "check the result");
}

{
  # 103786 - when filling up would cross a 4-connected border to the left
  # incorrectly
  my $im = Imager->new(xsize => 20, ysize => 20);
  $im->box(filled => 1, box => [ 0, 10, 9, 19 ], color => "FFFFFF");
  $im->box(filled => 1, box => [ 10, 0, 19, 9 ], color => "FFFFFF");
  my $cmp = $im->copy;
  $cmp->box(filled => 1, box => [ 10, 10, 19, 19 ], color => "0000FF");
  ok($im->flood_fill(x => 19, y => 19, color => "0000FF"),
     "flood_fill() to big checks");
  is_image($im, $cmp, "check result correct");
}

{
  # keys for tests are:
  #  name - base name of the test, the fill position is added
  #  boxes - arrayref of boxes to draw
  #  fillats - positions to start filling from
  # all of the tests must fill all of the image except that covered by
  # boxes
  my @tests =
    (
     {
      name => "1-pixel border",
      boxes => [ [ 1, 1, 18, 18 ] ],
      fillats =>
      [
       [ 0, 0 ],
       [ 19, 0 ],
       [ 0, 19 ],
       [ 19, 19 ],
       [ 10, 0 ],
       [ 10, 19 ],
       [ 0, 10 ],
       [ 19, 10 ],
      ]
     },
     {
      name => "vertical connect check",
      boxes =>
      [
       [ 0, 0, 8, 11 ],
       [ 10, 8, 19, 19 ],
      ],
      fillats =>
      [
       [ 19, 0 ],
       [ 0, 19 ],
      ],
     },
     {
      name => "horizontal connect check",
      boxes =>
      [
       [ 0, 0, 11, 8 ],
       [ 10, 10, 19, 19 ],
      ],
      fillats =>
      [
       [ 19, 0 ],
       [ 0, 19 ],
      ],
     },
    );

  my $box_color = Imager::Color->new("FF0000");
  my $fill_color = Imager::Color->new("00FF00");
  for my $test (@tests) {
    my $base_name = $test->{name};
    my $boxes = $test->{boxes};
    my $fillats = $test->{fillats};
    for my $pos (@$fillats) {
      for my $flip ("none", "h", "v", "vh") {
	my ($fillx, $filly) = @$pos;

	my $im = Imager->new(xsize => 20, ysize => 20);
	my $cmp = Imager->new(xsize => 20, ysize => 20);
	$cmp->box(filled => 1, color => $fill_color);
	for my $image ($im, $cmp) {
	  for my $box (@$boxes) {
	    $image->box(filled => 1, color => $box_color, box => $box );
	  }
	}
	if ($flip ne "none") {
	  $_->flip(dir => $flip) for $im, $cmp;
	  $flip =~ /h/ and $fillx = 19 - $fillx;
	  $flip =~ /v/ and $filly = 19 - $filly;
	}
	ok($im->flood_fill(x => $fillx, y => $filly, color => $fill_color),
	   "$base_name - \@($fillx,$filly) - flip $flip - fill");
	is_image($im, $cmp, "$base_name - \@($fillx,$filly) - flip $flip - compare");
      }
    }
  }
}

unless ($ENV{IMAGER_KEEP_FILES}) {
  unlink "testout/t22fill1.ppm";
  unlink "testout/t22fill2.ppm";
}

# make a vertical test image
sub vimage {
  my $c = shift;

  my $im = Imager->new(xsize => 10, ysize => 10);
  $im->line(x1 => 1, y1 => 1, x2 => 8, y2 => 1, color => $c);
  $im->line(x1 => 4, y1 => 2, x2 => 4, y2 => 8, color => $c);

  return $im;
}
