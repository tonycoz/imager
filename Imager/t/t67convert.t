Imager::init(log=>'testout/t67convert.log');

use Imager qw(:all :handy);

print "1..4\n";

my $imbase = Imager::ImgRaw::new(200,300,3);

# first a basic test, make sure the basic things happen ok
# make a 1 channel image from the above (black) image
# but with 1 as the 'extra' value
my $imnew = Imager::i_img_new();
unless (i_convert($imnew, $imbase, [ [ 0, 0, 0, 1 ] ])) {
  print "not ok 1 # call failed\n";
  print "ok 2 # skipped\n";
  print "ok 3 # skipped\n";
}
else {
  print "ok 1\n";
  my ($w, $h, $ch) = i_img_info($imnew);

  # the output image should now have one channel
  if ($ch == 1) {
    print "ok 2\n";
  }
  else {
    print "not ok 2 # $ch channels in output\n";
  }
  # should have the same width and height
  if ($w == 200 && $h == 300) {
    print "ok 3\n";
  }
  else {
    print "not ok 3 # output image is the wrong size!\n";
  }
  # should be a white image now, let's check
  my $c = Imager::i_get_pixel($imnew, 20, 20);
  my @c = $c->rgba;
  print "# @c\n";
  if (($c->rgba())[0] == 255) {
    print "ok 4\n";
  }
  else {
    print "not ok 4 # wrong colour in output image",($c->rgba())[0],"\n";
  }
}

# test the highlevel interface
# currently this requires visual inspection of the output files
my $im = Imager->new;
if ($im->read(file=>'testimg/scale.ppm')) {
  my $out;
  $out = $im->convert(preset=>'gray')
    or die "Cannot convert to gray:", $im->errstr;
  # FIXME we can't save 1 channel ppm files yet
  $out->write(file=>'testout/t67_gray.ppm', type=>'pnm')
    or print "# Cannot save testout/t67_gray.ppm:", $out->errstr;
  $out = $im->convert(preset=>'blue')
    or die "Cannot convert blue:", $im->errstr;
  # FIXME we can't save 1 channel ppm files yet
  $out->write(file=>'testout/t67_blue.ppm', type=>'pnm')
    or print "# Cannot save testout/t67_blue.ppm:", $out->errstr;
}
else {
  die "could not load testout/scale.ppm\n";
}
