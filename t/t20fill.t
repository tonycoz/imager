#!perl -w
use strict;

use Imager ':handy';
use Imager::Fill;
use Imager::Color::Float;

Imager::init_log("testout/t20fill.log", 1);

print "1..18\n";

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

my $solid = Imager::Fill->new(solid=>$red);
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
                 bg=>NC(128, 64, 0) });
$im->arc(r=>80, d1=>45, d2=>75, 
           fill=>{ hatch=>'stipple2',
                   combine=>1,
                   fg=>NC(0, 0, 0, 255),
                   bg=>NC(255,255,255,192) });
$im->arc(r=>80, d1=>75, d2=>135,
         fill=>{ fountain=>'radial', xa=>100, ya=>100, xb=>20, yb=>100 });
$im->write(file=>'testout/t20_sample.ppm');

sub ok {
  my ($num, $test, $desc) = @_;

  if ($test) {
    print "ok $num\n";
  }
  else {
    print "not ok $num # $desc\n";
  }
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
