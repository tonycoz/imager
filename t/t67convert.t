#!perl -w
use strict;
use Imager qw(:all :handy);
use lib 't';
use Test::More tests=>19;

Imager::init("log"=>'testout/t67convert.log');

my $imbase = Imager::ImgRaw::new(200,300,3);

# first a basic test, make sure the basic things happen ok
# make a 1 channel image from the above (black) image
# but with 1 as the 'extra' value
my $imnew = Imager::i_img_new();
SKIP:
{
  skip("convert to white failed", 3)
    unless ok(i_convert($imnew, $imbase, [ [ 0, 0, 0, 1 ] ]), "convert to white");

  my ($w, $h, $ch) = i_img_info($imnew);

  # the output image should now have one channel
  is($ch, 1, "one channel image now");
  # should have the same width and height
  ok($w == 200 && $h == 300, "check converted size is the same");

  # should be a white image now, let's check
  my $c = Imager::i_get_pixel($imnew, 20, 20);
  my @c = $c->rgba;
  print "# @c\n";
  is($c[0], 255, "check image is white");
}

# test the highlevel interface
# currently this requires visual inspection of the output files
my $im = Imager->new;
SKIP:
{
  skip("could not load scale.ppm", 3)
    unless $im->read(file=>'testimg/scale.ppm');
  my $out = $im->convert(preset=>'gray');
  ok($out, "convert preset gray");
  ok($out->write(file=>'testout/t67_gray.ppm', type=>'pnm'),
    "save grey image");
  $out = $im->convert(preset=>'blue');
  ok($out, "convert preset blue");

  ok($out->write(file=>'testout/t67_blue.ppm', type=>'pnm'),
     "save blue image");
}

# test against 16-bit/sample images
my $im16targ = Imager::i_img_16_new(200, 300, 3);
SKIP:
{
  skip("could not convert 16-bit image", 2)
    unless ok(i_convert($im16targ, $imbase, [ [ 0, 0, 0, 1 ],
                                              [ 0, 0, 0, 0 ],
                                              [ 0, 0, 0, 0 ] ]),
              "convert 16/bit sample image");
  # image should still be 16-bit
  is(Imager::i_img_bits($im16targ), 16, "Image still 16-bit/sample");
  # make sure that it's roughly red
  my $c = Imager::i_gpixf($im16targ, 0, 0);
  my @ch = $c->rgba;
  ok(abs($ch[0] - 1) <= 0.0001 && abs($ch[1]) <= 0.0001 && abs($ch[2]) <= 0.0001,
     "image roughly red");
}

# test against palette based images
my $impal = Imager::i_img_pal_new(200, 300, 3, 256);
my $black = NC(0, 0, 0);
my $blackindex = Imager::i_addcolors($impal, $black);
ok($blackindex, "add black to paletted");
for my $y (0..299) {
  Imager::i_ppal($impal, 0, $y, ($black) x 200);
}
my $impalout = Imager::i_img_pal_new(200, 300, 3, 256);
SKIP:
{
  skip("could not convert paletted", 3)
    unless ok(i_convert($impalout, $impal, [ [ 0, 0, 0, 0 ],
                                   [ 0, 0, 0, 1 ],
                                   [ 0, 0, 0, 0 ] ]),
             "convert paletted");
  is(Imager::i_img_type($impalout), 1, "image still paletted");
  is(Imager::i_colorcount($impalout), 1, "still only one colour");
  my $c = Imager::i_getcolors($impalout, $blackindex);
  ok($c, "get color from palette");
  my @ch = $c->rgba;
  print "# @ch\n";
  ok($ch[0] == 0 && $ch[1] == 255 && $ch[2] == 0, 
     "colour is as expected");
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
  $img->convert(preset=>"grey");
  cmp_ok($warning, '=~', 'void', "correct warning");
  cmp_ok($warning, '=~', 't67convert\\.t', "correct file");
}
