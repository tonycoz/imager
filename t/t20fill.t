#!perl -w
use strict;

print "1..39\n";

use Imager ':handy';
use Imager::Fill;
use Imager::Color::Float;

sub ok ($$$);

Imager::init_log("testout/t20fill.log", 1);

my $blue = NC(0,0,255);
my $red = NC(255, 0, 0);
my $redf = Imager::Color::Float->new(1, 0, 0);
my $rsolid = Imager::i_new_fill_solid($blue, 0);
ok(1, $rsolid, "building solid fill");
my $raw1 = Imager::ImgRaw::new(100, 100, 3);
# use the normal filled box
Imager::i_box_filled($raw1, 0, 0, 99, 99, $blue);
my $raw2 = Imager::ImgRaw::new(100, 100, 3);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rsolid);
ok(2, 1, "drawing with solid fill");
my $diff = Imager::i_img_diff($raw1, $raw2);
ok(3, $diff == 0, "solid fill doesn't match");
Imager::i_box_filled($raw1, 0, 0, 99, 99, $red);
my $rsolid2 = Imager::i_new_fill_solidf($redf, 0);
ok(4, $rsolid2, "creating float solid fill");
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rsolid2);
$diff = Imager::i_img_diff($raw1, $raw2);
ok(5, $diff == 0, "float solid fill doesn't match");

# ok solid still works, let's try a hatch
# hash1 is a 2x2 checkerboard
my $rhatcha = Imager::i_new_fill_hatch($red, $blue, 0, 1, undef, 0, 0);
my $rhatchb = Imager::i_new_fill_hatch($blue, $red, 0, 1, undef, 2, 0);
ok(6, $rhatcha && $rhatchb, "can't build hatched fill");

# the offset should make these match
Imager::i_box_cfill($raw1, 0, 0, 99, 99, $rhatcha);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rhatchb);
ok(7, 1, "filling with hatch");
$diff = Imager::i_img_diff($raw1, $raw2);
ok(8, $diff == 0, "hatch images different");
$rhatchb = Imager::i_new_fill_hatch($blue, $red, 0, 1, undef, 4, 6);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rhatchb);
$diff = Imager::i_img_diff($raw1, $raw2);
ok(9, $diff == 0, "hatch images different");

# I guess I was tired when I originally did this - make sure it keeps
# acting the way it's meant to
# I had originally expected these to match with the red and blue swapped
$rhatchb = Imager::i_new_fill_hatch($red, $blue, 0, 1, undef, 2, 2);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rhatchb);
$diff = Imager::i_img_diff($raw1, $raw2);
ok(10, $diff == 0, "hatch images different");

# this shouldn't match
$rhatchb = Imager::i_new_fill_hatch($red, $blue, 0, 1, undef, 1, 1);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rhatchb);
$diff = Imager::i_img_diff($raw1, $raw2);
ok(11, $diff, "hatch images the same!");

# custom hatch
# the inverse of the 2x2 checkerboard
my $hatch = pack("C8", 0x33, 0x33, 0xCC, 0xCC, 0x33, 0x33, 0xCC, 0xCC);
my $rcustom = Imager::i_new_fill_hatch($blue, $red, 0, 0, $hatch, 0, 0);
Imager::i_box_cfill($raw2, 0, 0, 99, 99, $rcustom);
$diff = Imager::i_img_diff($raw1, $raw2);
ok(12, !$diff, "custom hatch mismatch");

# test the oo interface
my $im1 = Imager->new(xsize=>100, ysize=>100);
my $im2 = Imager->new(xsize=>100, ysize=>100);

my $solid = Imager::Fill->new(solid=>'#FF0000');
ok(13, $solid, "creating oo solid fill");
ok(14, $solid->{fill}, "bad oo solid fill");
$im1->box(fill=>$solid);
$im2->box(filled=>1, color=>$red);
$diff = Imager::i_img_diff($im1->{IMG}, $im2->{IMG});
ok(15, !$diff, "oo solid fill");

my $hatcha = Imager::Fill->new(hatch=>'check2x2');
my $hatchb = Imager::Fill->new(hatch=>'check2x2', dx=>2);
$im1->box(fill=>$hatcha);
$im2->box(fill=>$hatchb);
# should be different
$diff = Imager::i_img_diff($im1->{IMG}, $im2->{IMG});
ok(16, $diff, "offset checks the same!");
$hatchb = Imager::Fill->new(hatch=>'check2x2', dx=>2, dy=>2);
$im2->box(fill=>$hatchb);
$diff = Imager::i_img_diff($im1->{IMG}, $im2->{IMG});
ok(17, !$diff, "offset into similar check should be the same");

# test dymanic build of fill
$im2->box(fill=>{hatch=>'check2x2', dx=>2, fg=>NC(255,255,255), 
                 bg=>NC(0,0,0)});
$diff = Imager::i_img_diff($im1->{IMG}, $im2->{IMG});
ok(18, !$diff, "offset and flipped should be the same");

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
ok(19, !$diff, "flood fill difference");

my $ffim = Imager->new(xsize=>100, ysize=>100);
my $yellow = Imager::Color->new(255, 255, 0);
$ffim->box(xmin=>10, ymin=>10, xmax=>20, ymax=>90, color=>$blue, filled=>1);
$ffim->box(xmin=>20, ymin=>45, xmax=>80, ymax=>55, color=>$blue, filled=>1);
$ffim->box(xmin=>80, ymin=>10, xmax=>90, ymax=>90, color=>$blue, filled=>1);
ok(20, $ffim->flood_fill('x'=>50, 'y'=>50, color=>$red), "flood fill");
$diff = Imager::i_img_diff($rffcmp, $ffim->{IMG});
ok(21, !$diff, "oo flood fill difference");
$ffim->flood_fill('x'=>50, 'y'=>50,
                  fill=> {
                          hatch => 'check2x2'
                         });
#                  fill=>{
#                         fountain=>'radial',
#                         xa=>50, ya=>50,
#                         xb=>10, yb=>10,
#                        });
$ffim->write(file=>'testout/t20_ooflood.ppm');

# test combining modes
my $fill = NC(192, 128, 128, 128);
my $target = NC(64, 32, 64);
my %comb_tests =
  (
   none=>{ result=>$fill },
   normal=>{ result=>NC(128, 80, 96) },
   multiply => { result=>NC(56, 24, 48) },
   dissolve => { result=>[ $target, NC(128, 80, 96) ] },
   add => { result=>NC(159, 96, 128) },
   subtract => { result=>NC(31, 15, 31) }, # 31.87, 15.9, 31.87
   diff => { result=>NC(96, 64, 64) },
   lighten => { result=>NC(128, 80, 96) },
   darken => { result=>$target },
   # the following results are based on the results of the tests and
   # are suspect for that reason (and were broken at one point <sigh>)
   # but trying to work them out manually just makes my head hurt - TC
   hue => { result=>NC(64, 32, 47) },
   saturation => { result=>NC(63, 37, 64) },
   value => { result=>NC(127, 64, 128) },
   color => { result=>NC(64, 37, 52) },
  );

my $testnum = 22; # from 22 to 34
for my $comb (Imager::Fill->combines) {
  my $test = $comb_tests{$comb};
  my $targim = Imager->new(xsize=>1, ysize=>1);
  $targim->box(filled=>1, color=>$target);
  my $fillobj = Imager::Fill->new(solid=>$fill, combine=>$comb);
  $targim->box(fill=>$fillobj);
  my $c = Imager::i_get_pixel($targim->{IMG}, 0, 0);
  if ($test->{result} =~ /ARRAY/) {
    ok($testnum++, scalar grep(color_close($_, $c), @{$test->{result}}), 
       "combine '$comb'")
      or print "# got:",join(",", $c->rgba),"  allowed: ", 
        join("|", map { join(",", $_->rgba) } @{$test->{result}}),"\n";
  }
  else {
    ok($testnum++, color_close($c, $test->{result}), "combine '$comb'")
      or print "# got: ",join(",", $c->rgba),
        "  allowed: ",join(",", $test->{result}->rgba),"\n";
  }
}

ok($testnum++, $ffim->arc(r=>45, color=>$blue, aa=>1), "aa circle");
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
ok($testnum++, 
   $oocopy->arc(fill=>{image=>$fillim, 
                       combine=>'normal',
                       xoff=>5}, r=>40),
   "image based fill");
$oocopy->write(file=>'testout/t20_image.ppm');

# a more complex version
use Imager::Matrix2d ':handy';
$oocopy = $ooim->copy;
ok($testnum++,
   $oocopy->arc(fill=>{
                       image=>$fillim,
                       combine=>'normal',
                       matrix=>m2d_rotate(degrees=>30),
                       xoff=>5
                       }, r=>40),
   "transformed image based fill");
$oocopy->write(file=>'testout/t20_image_xform.ppm');

ok($testnum++,
   !$oocopy->arc(fill=>{ hatch=>"not really a hatch" }, r=>20),
   "error handling of automatic fill conversion");
ok($testnum++,
   $oocopy->errstr =~ /Unknown hatch type/,
   "error message for automatic fill conversion");

sub ok ($$$) {
  my ($num, $test, $desc) = @_;

  if ($test) {
    print "ok $num\n";
  }
  else {
    print "not ok $num # $desc\n";
  }
  $test;
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
