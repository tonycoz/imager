#!perl -w
BEGIN { $| = 1; print "1..43\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;

$loaded = 1;

#$Imager::DEBUG=1;

Imager::init('log'=>'testout/t64copyflip.log');

$img=Imager->new() or die "unable to create image object\n";

$img->open(file=>'testimg/scale.ppm',type=>'pnm');
$nimg = $img->copy() or skip ( "\# warning ".$img->{'ERRSTR'}."\n" );
print "ok 1\n";

# test if ->copy() works

$diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
if ($diff > 0) { print "not ok 2 # copy and original differ!\n"; }
else { print "ok 2\n"; }


# test if ->flip(dir=>'h')->flip(dir=>'h') doesn't alter the image

$nimg->flip(dir=>"h")->flip(dir=>"h");
$diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
if ($diff > 0) { print "not ok 3 # double horizontal flip and original differ!\n"; }
else { print "ok 3\n"; }


# test if ->flip(dir=>'v')->flip(dir=>'v') doesn't alter the image

$nimg->flip(dir=>"v")->flip(dir=>"v");
$diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
if ($diff > 0) { print "not ok 4 # double vertical flip and original differ!\n"; }
else { print "ok 4\n"; }


# test if ->flip(dir=>'h')->flip(dir=>'v') is same as ->flip(dir=>'hv')

$nimg->flip(dir=>"v")->flip(dir=>"h")->flip(dir=>"hv");;
$diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
if ($diff > 0) {
  print "not ok 5 # double flips and original differ!\n";
  $nimg->write(file=>"testout/t64copyflip_error.ppm") or die $nimg->errstr();
}
else { print "ok 5\n"; }

rot_test(6, $img, 90, 4);
rot_test(10, $img, 180, 2);
rot_test(14, $img, 270, 4);
rot_test(18, $img, 0, 1);

my $pimg = $img->to_paletted();
rot_test(22, $pimg, 90, 4);
rot_test(26, $pimg, 180, 2);
rot_test(30, $pimg, 270, 4);
rot_test(34, $pimg, 0, 1);

my $timg = $img->rotate(right=>90)->rotate(right=>270);
Imager::i_img_diff($img->{IMG}, $timg->{IMG}) and print "not ";
print "ok 38\n";
$timg = $img->rotate(right=>90)->rotate(right=>180)->rotate(right=>90);
Imager::i_img_diff($img->{IMG}, $timg->{IMG}) and print "not ";
print "ok 39\n";

# this could use more tests
my $rimg = $img->rotate(degrees=>10)
  or print "not ";
print "ok 40\n";
if (!$rimg->write(file=>"testout/t64_rot10.ppm")) {
  print "# Cannot save: ",$rimg->errstr,"\n";
}

# rotate with background
$rimg = $img->rotate(degrees=>10, back=>Imager::Color->new(builtin=>'red'))
  or print "not ";
print "ok 41\n";
if (!$rimg->write(file=>"testout/t64_rot10_back.ppm")) {
  print "# Cannot save: ",$rimg->errstr,"\n";
}
	

my $trimg = $img->matrix_transform(matrix=>[ 1.2, 0, 0,
                                             0,   1, 0,
                                             0,   0, 1])
  or print "not ";
print "ok 42\n";
$trimg->write(file=>"testout/t64_trans.ppm")
  or print "# Cannot save: ",$trimg->errstr,"\n";

$trimg = $img->matrix_transform(matrix=>[ 1.2, 0, 0,
                                             0,   1, 0,
                                             0,   0, 1],
				   back=>Imager::Color->new(builtin=>'blue'))
  or print "not ";
print "ok 43\n";
$trimg->write(file=>"testout/t64_trans_back.ppm")
  or print "# Cannot save: ",$trimg->errstr,"\n";

sub rot_test {
  my ($testnum, $src, $degrees, $count) = @_;

  my $cimg = $src->copy();
  my $in;
  for (1..$count) {
    $in = $cimg;
    $cimg = $cimg->rotate(right=>$degrees)
      or last;
  }
  if ($cimg) {
    print "ok ",$testnum++,"\n";
    my $diff = Imager::i_img_diff($src->{IMG}, $cimg->{IMG});
    if ($diff) {
      print "not ok ",$testnum++," # final image doesn't match input\n";
      for (1..3) {
        print "ok ",$testnum++," # skipped\n";
      }
    }
    else {
      # check that other parameters match
      $src->type eq $cimg->type or print "not ";
      print "ok ",$testnum++," # type\n";
      $src->bits eq $cimg->bits or print "not ";
      print "ok ",$testnum++," # bits\n";
      $src->getchannels eq $cimg->getchannels or print "not ";
      print "ok ",$testnum++," # channels \n";
    }
  }
  else {
    print "not ok ",$testnum++," # rotate returned undef:",$in->errstr,"\n";
    for (1..4) {
      print "ok ",$testnum++," # skipped\n";
    }
  }
}

sub skip {
    print $_[0];
    print "ok 2 # skip\n";
    print "ok 3 # skip\n";
    print "ok 4 # skip\n";
    print "ok 5 # skip\n";
    exit(0);
}

