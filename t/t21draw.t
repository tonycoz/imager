# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)
use strict;
my $loaded;

BEGIN { $| = 1; print "1..29\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager qw/:all/;
$loaded = 1;
print "ok 1\n";

init_log("testout/t21draw.log",1);

my $redobj = NC(255, 0, 0);
my $red = 'FF0000';
my $greenobj = NC(0, 255, 0);
my $green = [ 0, 255, 0 ];
my $blueobj = NC(0, 0, 255);
my $blue = { hue=>240, saturation=>1, value=>1 };
my $white = '#FFFFFF';

my $testnum = 2;

my $img = Imager->new(xsize=>100, ysize=>100);

ok($img->box(color=>$blueobj, xmin=>10, ymin=>10, xmax=>48, ymax=>18),
   "box with color obj");
ok($img->box(color=>$blue, xmin=>10, ymin=>20, xmax=>48, ymax=>28),
   "box with color");
ok($img->box(color=>$redobj, xmin=>10, ymin=>30, xmax=>28, ymax=>48, filled=>1),
   "filled box with color obj");
ok($img->box(color=>$red, xmin=>30, ymin=>30, xmax=>48, ymax=>48, filled=>1),
   "filled box with color");

ok($img->arc('x'=>75, 'y'=>25, r=>24, color=>$redobj),
   "filled arc with colorobj");

ok($img->arc('x'=>75, 'y'=>25, r=>20, color=>$green),
   "filled arc with colorobj");
ok($img->arc('x'=>75, 'y'=>25, r=>18, color=>$white, d1=>325, d2=>225),
   "filled arc with color");

ok($img->arc('x'=>75, 'y'=>25, r=>18, color=>$blue, d1=>225, d2=>325),
   "filled arc with color");
ok($img->arc('x'=>75, 'y'=>25, r=>15, color=>$green, aa=>1),
   "filled arc with color");

ok($img->line(color=>$blueobj, x1=>5, y1=>55, x2=>35, y2=>95),
   "line with colorobj");

# FIXME - neither the start nor end-point is set for a non-aa line
#my $c = Imager::i_get_pixel($img->{IMG}, 5, 55);
#ok(color_cmp($c, $blueobj) == 0, "# TODO start point not set");

ok($img->line(color=>$red, x1=>10, y1=>55, x2=>40, y2=>95, aa=>1),
   "aa line with color");
ok($img->line(color=>$green, x1=>15, y1=>55, x2=>45, y2=>95, antialias=>1),
   "antialias line with color");

ok($img->polyline(points=>[ [ 55, 55 ], [ 90, 60 ], [ 95, 95] ],
                  color=>$redobj),
   "polyline points with color obj");
ok($img->polyline('x'=>[ 55, 85, 90 ], 'y'=>[60, 65, 95], color=>$green, aa=>1),
   "polyline xy with color aa");
ok($img->polyline('x'=>[ 55, 80, 85 ], 'y'=>[65, 70, 95], color=>$green, 
                  antialias=>1),
   "polyline xy with color antialias");

ok($img->setpixel('x'=>[35, 37, 39], 'y'=>[55, 57, 59], color=>$red),
   "set array of pixels");
ok($img->setpixel('x'=>39, 'y'=>55, color=>$green),
   "set single pixel");
use Imager::Color::Float;
my $flred = Imager::Color::Float->new(1, 0, 0, 0);
my $flgreen = Imager::Color::Float->new(0, 1, 0, 0);
ok($img->setpixel('x'=>[41, 43, 45], 'y'=>[55, 57, 59], color=>$flred),
   "set array of float pixels");
ok($img->setpixel('x'=>45, 'y'=>55, color=>$flgreen),
   "set single float pixel");
my @gp = $img->getpixel('x'=>[41, 43, 45], 'y'=>[55, 57, 59]);
ok(grep($_->isa('Imager::Color'), @gp) == 3, "check getpixel result type");
ok(grep(color_cmp($_, NC(255, 0, 0)) == 0, @gp) == 3, 
   "check getpixel result colors");
my $gp = $img->getpixel('x'=>45, 'y'=>55);
ok($gp->isa('Imager::Color'), "check scalar getpixel type");
ok(color_cmp($gp, NC(0, 255, 0)) == 0, "check scalar getpixel color");
@gp = $img->getpixel('x'=>[35, 37, 39], 'y'=>[55, 57, 59], type=>'float');
ok(grep($_->isa('Imager::Color::Float'), @gp) == 3, 
   "check getpixel float result type");
ok(grep(color_cmp($_, $flred) == 0, @gp) == 3,
   "check getpixel float result type");
$gp = $img->getpixel('x'=>39, 'y'=>55, type=>'float');
ok($gp->isa('Imager::Color::Float'), "check scalar float getpixel type");
ok(color_cmp($gp, $flgreen) == 0, "check scalar float getpixel color");

ok($img->write(file=>'testout/t21draw.ppm'),
   "saving output");

malloc_state();

sub ok {
  my ($ok, $msg) = @_;

  if ($ok) {
    print "ok ",$testnum++,"\n";
  }
  else {
    print "not ok ",$testnum++," # $msg\n";
  }
}

sub color_cmp {
  my ($l, $r) = @_;
  my @l = $l->rgba;
  my @r = $r->rgba;
  # print "# (",join(",", @l[0..2]),") <=> (",join(",", @r[0..2]),")\n";
  return $l[0] <=> $r[0]
    || $l[1] <=> $r[1]
      || $l[2] <=> $r[2];
}
