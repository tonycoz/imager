Imager::init("log"=>'testout/t67convert.log');

use Imager qw(:all :handy);

print "1..17\n";

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
  print "ok 5\n";
  my $out;
  $out = $im->convert(preset=>'gray')
    or print "not ";
  print "ok 6\n";
  if ($out->write(file=>'testout/t67_gray.ppm', type=>'pnm')) {
    print "ok 7\n";
  }
  else {
    print "not ok 7 # Cannot save testout/t67_gray.ppm:", $out->errstr,"\n";
  }
  $out = $im->convert(preset=>'blue')
    or print "not ";
  print "ok 8\n";

  if ($out->write(file=>'testout/t67_blue.ppm', type=>'pnm')) {
    print "ok 9\n";
  }
  else {
    print "not ok 9 # Cannot save testout/t67_blue.ppm:", $out->errstr, "\n";
  }
}
else {
  print "not ok 5 # could not load testout/scale.ppm\n";
  print map "ok $_ # skipped\n", 6..9;
}

# test against 16-bit/sample images
my $im16targ = Imager::i_img_16_new(200, 300, 3);
unless (i_convert($im16targ, $imbase, [ [ 0, 0, 0, 1 ],
                                        [ 0, 0, 0, 0 ],
                                        [ 0, 0, 0, 0 ] ])) {
  print "not ok 10 # call failed\n";
  print map "ok $_ # skipped\n", 11..12;
}
else {
  print "ok 10\n";

  # image should still be 16-bit
  Imager::i_img_bits($im16targ) == 16
      or print "not ";
  print "ok 11\n";
  # make sure that it's roughly red
  my $c = Imager::i_gpixf($im16targ, 0, 0);
  my @ch = $c->rgba;
  abs($ch[0] - 1) <= 0.0001 && abs($ch[1]) <= 0.0001 && abs($ch[2]) <= 0.0001
    or print "not ";
  print "ok 12\n";
}

# test against palette based images
my $impal = Imager::i_img_pal_new(200, 300, 3, 256);
my $black = NC(0, 0, 0);
my $blackindex = Imager::i_addcolors($impal, $black)
  or print "not ";
print "ok 13\n";
for my $y (0..299) {
  Imager::i_ppal($impal, 0, $y, ($black) x 200);
}
my $impalout = Imager::i_img_pal_new(200, 300, 3, 256);
if (i_convert($impalout, $impal, [ [ 0, 0, 0, 0 ],
                                   [ 0, 0, 0, 1 ],
                                   [ 0, 0, 0, 0 ] ])) {
  Imager::i_img_type($impalout) == 1 or print "not ";
  print "ok 14\n";
  Imager::i_colorcount($impalout) == 1 or print "not ";
  print "ok 15\n";
  my $c = Imager::i_getcolors($impalout, $blackindex) or print "not ";
  print "ok 16\n";
  my @ch = $c->rgba;
  print "# @ch\n";
  $ch[0] == 0 && $ch[1] == 255 && $ch[2] == 0
    or print "not ";
  print "ok 17\n";
}
else {
  print "not ok 14 # could not convert paletted image\n";
  print map "ok $_ # skipped\n", 15..17;
}
