#!perl -w
use strict;
use Test::More tests => 121;

use Imager ':handy';
use Imager::Fill;
use Imager::Color::Float;
use Config;

Imager::init_log("testout/t20fill.log", 1);

my $blue = NC(0,0,255);
my $red = NC(255, 0, 0);
my $redf = Imager::Color::Float->new(1, 0, 0);
my $bluef = Imager::Color::Float->new(0, 0, 1);
my $rsolid = Imager::i_new_fill_solid($blue, 0);
ok($rsolid, "building solid fill");
my $raw1 = Imager::ImgRaw::new(100, 100, 3);
# use the normal filled box
Imager::i_box_filled($raw1, 0, 0, 99, 99, $blue);
my $raw2 = Imager::ImgRaw::new(100, 100, 3);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rsolid);
ok(1, "drawing with solid fill");
my $diff = Imager::i_img_diff($raw1, $raw2);
ok($diff == 0, "solid fill doesn't match");
Imager::i_box_filled($raw1, 0, 0, 99, 99, $red);
my $rsolid2 = Imager::i_new_fill_solidf($redf, 0);
ok($rsolid2, "creating float solid fill");
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rsolid2);
$diff = Imager::i_img_diff($raw1, $raw2);
ok($diff == 0, "float solid fill doesn't match");

# ok solid still works, let's try a hatch
# hash1 is a 2x2 checkerboard
my $rhatcha = Imager::i_new_fill_hatch($red, $blue, 0, 1, undef, 0, 0);
my $rhatchb = Imager::i_new_fill_hatch($blue, $red, 0, 1, undef, 2, 0);
ok($rhatcha && $rhatchb, "can't build hatched fill");

# the offset should make these match
Imager::i_box_cfill($raw1, 0, 0, 99, 99, $rhatcha);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rhatchb);
ok(1, "filling with hatch");
$diff = Imager::i_img_diff($raw1, $raw2);
ok($diff == 0, "hatch images different");
$rhatchb = Imager::i_new_fill_hatch($blue, $red, 0, 1, undef, 4, 6);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rhatchb);
$diff = Imager::i_img_diff($raw1, $raw2);
ok($diff == 0, "hatch images different");

# I guess I was tired when I originally did this - make sure it keeps
# acting the way it's meant to
# I had originally expected these to match with the red and blue swapped
$rhatchb = Imager::i_new_fill_hatch($red, $blue, 0, 1, undef, 2, 2);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rhatchb);
$diff = Imager::i_img_diff($raw1, $raw2);
ok($diff == 0, "hatch images different");

# this shouldn't match
$rhatchb = Imager::i_new_fill_hatch($red, $blue, 0, 1, undef, 1, 1);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rhatchb);
$diff = Imager::i_img_diff($raw1, $raw2);
ok($diff, "hatch images the same!");

# custom hatch
# the inverse of the 2x2 checkerboard
my $hatch = pack("C8", 0x33, 0x33, 0xCC, 0xCC, 0x33, 0x33, 0xCC, 0xCC);
my $rcustom = Imager::i_new_fill_hatch($blue, $red, 0, 0, $hatch, 0, 0);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rcustom);
$diff = Imager::i_img_diff($raw1, $raw2);
ok(!$diff, "custom hatch mismatch");

{
  # basic test of floating color hatch fills
  # this will exercise the code that the gcc shipped with OS X 10.4
  # forgets to generate
  # the float version is called iff we're working with a non-8-bit image
  # i_new_fill_hatchf() makes the same object as i_new_fill_hatch() but
  # we test the other construction code path here
  my $fraw1 = Imager::i_img_double_new(100, 100, 3);
  my $fhatch1 = Imager::i_new_fill_hatchf($redf, $bluef, 0, 1, undef, 0, 0);
  ok($fraw1, "making double image 1");
  ok($fhatch1, "making float hatch 1");
  Imager::i_box_cfill($fraw1, 0, 0, 99, 99, $fhatch1);
  my $fraw2 = Imager::i_img_double_new(100, 100, 3);
  my $fhatch2 = Imager::i_new_fill_hatchf($bluef, $redf, 0, 1, undef, 0, 2);
  ok($fraw2, "making double image 2");
  ok($fhatch2, "making float hatch 2");
  Imager::i_box_cfill($fraw2, 0, 0, 99, 99, $fhatch2);

  $diff = Imager::i_img_diff($fraw1, $fraw2);
  ok(!$diff, "float custom hatch mismatch");
  save($fraw1, "testout/t20hatchf1.ppm");
  save($fraw2, "testout/t20hatchf2.ppm");
}

# test the oo interface
my $im1 = Imager->new(xsize=>100, ysize=>100);
my $im2 = Imager->new(xsize=>100, ysize=>100);

my $solid = Imager::Fill->new(solid=>'#FF0000');
ok($solid, "creating oo solid fill");
ok($solid->{fill}, "bad oo solid fill");
$im1->box(fill=>$solid);
$im2->box(filled=>1, color=>$red);
$diff = Imager::i_img_diff($im1->{IMG}, $im2->{IMG});
ok(!$diff, "oo solid fill");

my $hatcha = Imager::Fill->new(hatch=>'check2x2');
my $hatchb = Imager::Fill->new(hatch=>'check2x2', dx=>2);
$im1->box(fill=>$hatcha);
$im2->box(fill=>$hatchb);
# should be different
$diff = Imager::i_img_diff($im1->{IMG}, $im2->{IMG});
ok($diff, "offset checks the same!");
$hatchb = Imager::Fill->new(hatch=>'check2x2', dx=>2, dy=>2);
$im2->box(fill=>$hatchb);
$diff = Imager::i_img_diff($im1->{IMG}, $im2->{IMG});
ok(!$diff, "offset into similar check should be the same");

# test dymanic build of fill
$im2->box(fill=>{hatch=>'check2x2', dx=>2, fg=>NC(255,255,255), 
                 bg=>NC(0,0,0)});
$diff = Imager::i_img_diff($im1->{IMG}, $im2->{IMG});
ok(!$diff, "offset and flipped should be the same");

# a simple demo
my $im = Imager->new(xsize=>200, ysize=>200);

$im->box(xmin=>10, ymin=>10, xmax=>190, ymax=>190,
         fill=>{ hatch=>'check4x4',
                 fg=>NC(128, 0, 0),
                 bg=>NC(128, 64, 0) })
  or print "# ",$im->errstr,"\n";
$im->arc(r=>80, d1=>45, d2=>75, 
           fill=>{ hatch=>'stipple2',
                   combine=>1,
                   fg=>[ 0, 0, 0, 255 ],
                   bg=>{ rgba=>[255,255,255,160] } })
  or print "# ",$im->errstr,"\n";
$im->arc(r=>80, d1=>75, d2=>135,
         fill=>{ fountain=>'radial', xa=>100, ya=>100, xb=>20, yb=>100 })
  or print "# ",$im->errstr,"\n";
$im->write(file=>'testout/t20_sample.ppm');

# flood fill tests
my $rffimg = Imager::ImgRaw::new(100, 100, 3);
# build a H 
Imager::i_box_filled($rffimg, 10, 10, 20, 90, $blue);
Imager::i_box_filled($rffimg, 80, 10, 90, 90, $blue);
Imager::i_box_filled($rffimg, 20, 45, 80, 55, $blue);
my $black = Imager::Color->new(0, 0, 0);
Imager::i_flood_fill($rffimg, 15, 15, $red);
my $rffcmp = Imager::ImgRaw::new(100, 100, 3);
# build a H 
Imager::i_box_filled($rffcmp, 10, 10, 20, 90, $red);
Imager::i_box_filled($rffcmp, 80, 10, 90, 90, $red);
Imager::i_box_filled($rffcmp, 20, 45, 80, 55, $red);
$diff = Imager::i_img_diff($rffimg, $rffcmp);
ok(!$diff, "flood fill difference");

my $ffim = Imager->new(xsize=>100, ysize=>100);
my $yellow = Imager::Color->new(255, 255, 0);
$ffim->box(xmin=>10, ymin=>10, xmax=>20, ymax=>90, color=>$blue, filled=>1);
$ffim->box(xmin=>20, ymin=>45, xmax=>80, ymax=>55, color=>$blue, filled=>1);
$ffim->box(xmin=>80, ymin=>10, xmax=>90, ymax=>90, color=>$blue, filled=>1);
ok($ffim->flood_fill('x'=>50, 'y'=>50, color=>$red), "flood fill");
$diff = Imager::i_img_diff($rffcmp, $ffim->{IMG});
ok(!$diff, "oo flood fill difference");
$ffim->flood_fill('x'=>50, 'y'=>50,
                  fill=> {
                          hatch => 'check2x2',
			  fg => '0000FF',
                         });
#                  fill=>{
#                         fountain=>'radial',
#                         xa=>50, ya=>50,
#                         xb=>10, yb=>10,
#                        });
$ffim->write(file=>'testout/t20_ooflood.ppm');

my $copy = $ffim->copy;
ok($ffim->flood_fill('x' => 50, 'y' => 50,
		     color => $red, border => '000000'),
   "border solid flood fill");
is(Imager::i_img_diff($ffim->{IMG}, $rffcmp), 0, "compare");
ok($ffim->flood_fill('x' => 50, 'y' => 50,
		     fill => { hatch => 'check2x2', fg => '0000FF', },
		     border => '000000'),
   "border cfill fill");
is(Imager::i_img_diff($ffim->{IMG}, $copy->{IMG}), 0,
   "compare");

# test combining modes
my $fill = NC(192, 128, 128, 128);
my $target = NC(64, 32, 64);
my $trans_target = NC(64, 32, 64, 128);
my %comb_tests =
  (
   none=>
   { 
    opaque => $fill,
    trans => $fill,
   },
   normal=>
   { 
    opaque => NC(128, 80, 96),
    trans => NC(150, 96, 107, 191),
   },
   multiply => 
   { 
    opaque => NC(56, 24, 48),
    trans => NC(101, 58, 74, 192),
   },
   dissolve => 
   { 
    opaque => [ $target, NC(192, 128, 128, 255) ],
    trans => [ $trans_target, NC(192, 128, 128, 255) ],
   },
   add => 
   { 
    opaque => NC(159, 96, 128),
    trans => NC(128, 80, 96, 255),
   },
   subtract => 
   { 
    opaque => NC(0, 0, 0),
    trans => NC(0, 0, 0, 255),
   },
   diff => 
   { 
    opaque => NC(96, 64, 64),
    trans => NC(127, 85, 85, 192),
   },
   lighten => 
   { 
    opaque => NC(128, 80, 96), 
    trans => NC(149, 95, 106, 192), 
   },
   darken => 
   { 
    opaque => $target,
    trans => NC(106, 63, 85, 192),
   },
   # the following results are based on the results of the tests and
   # are suspect for that reason (and were broken at one point <sigh>)
   # but trying to work them out manually just makes my head hurt - TC
   hue => 
   { 
    opaque => NC(64, 32, 47),
    trans => NC(64, 32, 42, 128),
   },
   saturation => 
   { 
    opaque => NC(63, 37, 64),
    trans => NC(64, 39, 64, 128),
   },
   value => 
   { 
    opaque => NC(127, 64, 128),
    trans => NC(149, 75, 150, 128),
   },
   color => 
   { 
    opaque => NC(64, 37, 52),
    trans => NC(64, 39, 50, 128),
   },
  );

for my $comb (Imager::Fill->combines) {
  my $test = $comb_tests{$comb};
  my $fillobj = Imager::Fill->new(solid=>$fill, combine=>$comb);

  for my $bits (qw(8 double)) {
    {
      my $targim = Imager->new(xsize=>4, ysize=>4, bits => $bits);
      $targim->box(filled=>1, color=>$target);
      $targim->box(fill=>$fillobj);
      my $c = Imager::i_get_pixel($targim->{IMG}, 1, 1);
      my $allowed = $test->{opaque};
      $allowed =~ /ARRAY/ or $allowed = [ $allowed ];
      ok(scalar grep(color_close($_, $c), @$allowed), 
	 "opaque '$comb' $bits bits")
	or print "# got:",join(",", $c->rgba),"  allowed: ", 
	  join("|", map { join(",", $_->rgba) } @$allowed),"\n";
    }
    
    {
      # make sure the alpha path in the combine function produces the same
      # or at least as sane a result as the non-alpha path
      my $targim = Imager->new(xsize=>4, ysize=>4, channels => 4, bits => $bits);
      $targim->box(filled=>1, color=>$target);
      $targim->box(fill=>$fillobj);
      my $c = Imager::i_get_pixel($targim->{IMG}, 1, 1);
      my $allowed = $test->{opaque};
      $allowed =~ /ARRAY/ or $allowed = [ $allowed ];
      ok(scalar grep(color_close4($_, $c), @$allowed), 
	 "opaque '$comb' 4-channel $bits bits")
	or print "# got:",join(",", $c->rgba),"  allowed: ", 
	  join("|", map { join(",", $_->rgba) } @$allowed),"\n";
    }
    
    {
      my $transim = Imager->new(xsize => 4, ysize => 4, channels => 4, bits => $bits);
      $transim->box(filled=>1, color=>$trans_target);
      $transim->box(fill => $fillobj);
      my $c = $transim->getpixel(x => 1, 'y' => 1);
      my $allowed = $test->{trans};
      $allowed =~ /ARRAY/ or $allowed = [ $allowed ];
      ok(scalar grep(color_close4($_, $c), @$allowed), 
	 "translucent '$comb' $bits bits")
	or print "# got:",join(",", $c->rgba),"  allowed: ", 
	  join("|", map { join(",", $_->rgba) } @$allowed),"\n";
    }
  }
}

ok($ffim->arc(r=>45, color=>$blue, aa=>1), "aa circle");
$ffim->write(file=>"testout/t20_aacircle.ppm");

# image based fills
my $green = NC(0, 255, 0);
my $fillim = Imager->new(xsize=>40, ysize=>40, channels=>4);
$fillim->box(filled=>1, xmin=>5, ymin=>5, xmax=>35, ymax=>35, 
             color=>NC(0, 0, 255, 128));
$fillim->arc(filled=>1, r=>10, color=>$green, aa=>1);
my $ooim = Imager->new(xsize=>150, ysize=>150);
$ooim->box(filled=>1, color=>$green, xmin=>70, ymin=>25, xmax=>130, ymax=>125);
$ooim->box(filled=>1, color=>$blue, xmin=>20, ymin=>25, xmax=>80, ymax=>125);
$ooim->arc(r=>30, color=>$red, aa=>1);

my $oocopy = $ooim->copy();
ok($oocopy->arc(fill=>{image=>$fillim, 
                       combine=>'normal',
                       xoff=>5}, r=>40),
   "image based fill");
$oocopy->write(file=>'testout/t20_image.ppm');

# a more complex version
use Imager::Matrix2d ':handy';
$oocopy = $ooim->copy;
ok($oocopy->arc(fill=>{
                       image=>$fillim,
                       combine=>'normal',
                       matrix=>m2d_rotate(degrees=>30),
                       xoff=>5
                       }, r=>40),
   "transformed image based fill");
$oocopy->write(file=>'testout/t20_image_xform.ppm');

ok(!$oocopy->arc(fill=>{ hatch=>"not really a hatch" }, r=>20),
   "error handling of automatic fill conversion");
ok($oocopy->errstr =~ /Unknown hatch type/,
   "error message for automatic fill conversion");

# previous box fills to float images, or using the fountain fill
# got into a loop here

SKIP:
{
  skip("can't test without alarm()", 1) unless $Config{d_alarm};
  local $SIG{ALRM} = sub { die; };

  eval {
    alarm(2);
    ok($ooim->box(xmin=>20, ymin=>20, xmax=>80, ymax=>40,
                  fill=>{ fountain=>'linear', xa=>20, ya=>20, xb=>80, 
                          yb=>20 }), "linear box fill");
    alarm 0;
  };
  $@ and ok(0, "linear box fill $@");
}

# test that passing in a non-array ref returns an error
{
  my $fill = Imager::Fill->new(fountain=>'linear',
                               xa => 20, ya=>20, xb=>20, yb=>40,
                               segments=>"invalid");
  ok(!$fill, "passing invalid segments produces an error");
  cmp_ok(Imager->errstr, '=~', 'array reference',
         "check the error message");
}

# test that colors in segments are converted
{
  my @segs =
    (
     [ 0.0, 0.5, 1.0, '000000', '#FFF', 0, 0 ],
    );
  my $fill = Imager::Fill->new(fountain=>'linear',
                               xa => 0, ya=>20, xb=>49, yb=>20,
                               segments=>\@segs);
  ok($fill, "check that color names are converted")
    or print "# ",Imager->errstr,"\n";
  my $im = Imager->new(xsize=>50, ysize=>50);
  $im->box(fill=>$fill);
  my $left = $im->getpixel('x'=>0, 'y'=>20);
  ok(color_close($left, Imager::Color->new(0,0,0)),
     "check black converted correctly");
  my $right = $im->getpixel('x'=>49, 'y'=>20);
  ok(color_close($right, Imager::Color->new(255,255,255)),
     "check white converted correctly");

  # check that invalid colors handled correctly
  
  my @segs2 =
    (
     [ 0.0, 0.5, 1.0, '000000', 'FxFxFx', 0, 0 ],
    );
  my $fill2 = Imager::Fill->new(fountain=>'linear',
                               xa => 0, ya=>20, xb=>49, yb=>20,
                               segments=>\@segs2);
  ok(!$fill2, "check handling of invalid color names");
  cmp_ok(Imager->errstr, '=~', 'No color named', "check error message");
}

sub color_close {
  my ($c1, $c2) = @_;

  my @c1 = $c1->rgba;
  my @c2 = $c2->rgba;

  for my $i (0..2) {
    if (abs($c1[$i]-$c2[$i]) > 2) {
      return 0;
    }
  }
  return 1;
}

sub color_close4 {
  my ($c1, $c2) = @_;

  my @c1 = $c1->rgba;
  my @c2 = $c2->rgba;

  for my $i (0..3) {
    if (abs($c1[$i]-$c2[$i]) > 2) {
      return 0;
    }
  }
  return 1;
}

# for use during testing
sub save {
  my ($im, $name) = @_;

  open FH, "> $name" or die "Cannot create $name: $!";
  binmode FH;
  my $io = Imager::io_new_fd(fileno(FH));
  Imager::i_writeppm_wiol($im, $io) or die "Cannot save to $name";
  undef $io;
  close FH;
}
