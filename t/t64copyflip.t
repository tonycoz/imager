#!perl -w
use strict;
use Test::More tests => 65;
use Imager;
use Imager::Test qw(is_color3);

#$Imager::DEBUG=1;

Imager::init('log'=>'testout/t64copyflip.log');

my $img=Imager->new() or die "unable to create image object\n";

$img->open(file=>'testimg/scale.ppm',type=>'pnm');
my $nimg = $img->copy();
ok($nimg, "copy returned something");

# test if ->copy() works

my $diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
is($diff, 0, "copy matches source");


# test if ->flip(dir=>'h')->flip(dir=>'h') doesn't alter the image

$nimg->flip(dir=>"h")->flip(dir=>"h");
$diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
is($diff, 0, "double horiz flipped matches original");

# test if ->flip(dir=>'v')->flip(dir=>'v') doesn't alter the image

$nimg->flip(dir=>"v")->flip(dir=>"v");
$diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
is($diff, 0, "double vertically flipped image matches original");


# test if ->flip(dir=>'h')->flip(dir=>'v') is same as ->flip(dir=>'hv')

$nimg->flip(dir=>"v")->flip(dir=>"h")->flip(dir=>"hv");;
$diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
is($diff, 0, "check flip with hv matches flip v then flip h");

rot_test($img, 90, 4);
rot_test($img, 180, 2);
rot_test($img, 270, 4);
rot_test($img, 0, 1);

my $pimg = $img->to_paletted();
rot_test($pimg, 90, 4);
rot_test($pimg, 180, 2);
rot_test($pimg, 270, 4);
rot_test($pimg, 0, 1);

my $timg = $img->rotate(right=>90)->rotate(right=>270);
is(Imager::i_img_diff($img->{IMG}, $timg->{IMG}), 0,
   "check rotate 90 then 270 matches original");
$timg = $img->rotate(right=>90)->rotate(right=>180)->rotate(right=>90);
is(Imager::i_img_diff($img->{IMG}, $timg->{IMG}), 0,
     "check rotate 90 then 180 then 90 matches original");

# this could use more tests
my $rimg = $img->rotate(degrees=>10);
ok($rimg, "rotation by 10 degrees gave us an image");
if (!$rimg->write(file=>"testout/t64_rot10.ppm")) {
  print "# Cannot save: ",$rimg->errstr,"\n";
}

# rotate with background
$rimg = $img->rotate(degrees=>10, back=>Imager::Color->new(builtin=>'red'));
ok($rimg, "rotate with background gave us an image");
if (!$rimg->write(file=>"testout/t64_rot10_back.ppm")) {
  print "# Cannot save: ",$rimg->errstr,"\n";
}

{
  # rotate with text background
  my $rimg = $img->rotate(degrees => 45, back => '#FF00FF');
  ok($rimg, "rotate with background as text gave us an image");
  
  # check the color set correctly
  my $c = $rimg->getpixel(x => 0, 'y' => 0);
  is_deeply([ 255, 0, 255 ], [ ($c->rgba)[0, 1, 2] ],
            "check background set correctly");

  # check error handling for background color
  $rimg = $img->rotate(degrees => 45, back => "some really unknown color");
  ok(!$rimg, "should fail due to bad back color");
  cmp_ok($img->errstr, '=~', "^No color named ", "check error message");
}

my $trimg = $img->matrix_transform(matrix=>[ 1.2, 0, 0,
                                             0,   1, 0,
                                             0,   0, 1]);
ok($trimg, "matrix_transform() returned an image");
$trimg->write(file=>"testout/t64_trans.ppm")
  or print "# Cannot save: ",$trimg->errstr,"\n";

$trimg = $img->matrix_transform(matrix=>[ 1.2, 0, 0,
                                             0,   1, 0,
                                             0,   0, 1],
				   back=>Imager::Color->new(builtin=>'blue'));
ok($trimg, "matrix_transform() with back returned an image");

$trimg->write(file=>"testout/t64_trans_back.ppm")
  or print "# Cannot save: ",$trimg->errstr,"\n";

sub rot_test {
  my ($src, $degrees, $count) = @_;

  my $cimg = $src->copy();
  my $in;
  for (1..$count) {
    $in = $cimg;
    $cimg = $cimg->rotate(right=>$degrees)
      or last;
  }
 SKIP:
  {
    ok($cimg, "got a rotated image")
      or skip("no image to check", 4);
    my $diff = Imager::i_img_diff($src->{IMG}, $cimg->{IMG});
    is($diff, 0, "check it matches source")
      or skip("didn't match", 3);

    # check that other parameters match
    is($src->type, $cimg->type, "type check");
    is($src->bits, $cimg->bits, "bits check");
    is($src->getchannels, $cimg->getchannels, "channels check");
  }
}

{ # http://rt.cpan.org/NoAuth/Bug.html?id=9672
  my $warning;
  local $SIG{__WARN__} = 
    sub { 
      $warning = "@_";
      my $printed = $warning;
      $printed =~ s/\n$//;
      $printed =~ s/\n/\n\#/g; 
      print "# ",$printed, "\n";
    };
  my $img = Imager->new(xsize=>10, ysize=>10);
  $img->copy();
  cmp_ok($warning, '=~', 'void', "correct warning");
  cmp_ok($warning, '=~', 't64copyflip\\.t', "correct file");
  $warning = '';
  $img->rotate(degrees=>5);
  cmp_ok($warning, '=~', 'void', "correct warning");
  cmp_ok($warning, '=~', 't64copyflip\\.t', "correct file");
  $warning = '';
  $img->matrix_transform(matrix=>[1, 1, 1]);
  cmp_ok($warning, '=~', 'void', "correct warning");
  cmp_ok($warning, '=~', 't64copyflip\\.t', "correct file");
}

{
  # 29936 - matrix_transform() should use fabs() instead of abs()
  # range checking sz 

  # this meant that when sz was < 1 (which it often is for these
  # transformations), it treated the values out of range, producing a
  # blank output image

  my $src = Imager->new(xsize => 20, ysize => 20);
  $src->box(filled => 1, color => 'FF0000');
  my $out = $src->matrix_transform(matrix => [ 1, 0, 0,
					       0, 1, 0,
					       0, 0, 0.9999 ])
    or print "# ", $src->errstr, "\n";
  my $blank = Imager->new(xsize => 20, ysize => 20);
  # they have to be different, surely that would be easy
  my $diff = Imager::i_img_diff($out->{IMG}, $blank->{IMG});
  ok($diff, "RT#29936 - check non-blank output");
}

{
  my $im = Imager->new(xsize => 10, ysize => 10, channels => 4);
  $im->box(filled => 1, color => 'FF0000');
  my $back = Imager::Color->new(0, 0, 0, 0);
  my $rot = $im->rotate(degrees => 10, back => $back);
  # drop the alpha and make sure there's only 2 colors used
  my $work = $rot->convert(preset => 'noalpha');
  my $im_pal = $work->to_paletted(make_colors => 'mediancut');
  my @colors = $im_pal->getcolors;
  is(@colors, 2, "should be only 2 colors");
  @colors = sort { ($a->rgba)[0] <=> ($b->rgba)[0] } @colors;
  is_color3($colors[0], 0, 0, 0, "check we got black");
  is_color3($colors[1], 255, 0, 0, "and red");
}
