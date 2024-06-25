#!perl -w
use strict;
use Imager qw(:all :handy);
use Test::More;
use Imager::Test qw(test_colorf_gpix is_fcolor1 is_fcolor3 is_arrayf test_image
                    to_linear_srgb to_gamma_srgb to_linear_srgbf to_gamma_srgbf);

-d "testout" or mkdir "testout";

Imager::init("log"=>'testout/t67convert.log');

{
  # test convesion from the supplied matrices to the data supplied to
  # the underlying i_convert() implementation
  # Each test has:
  #  Inputs:
  #    name - base name of the test
  #    main - main channels array ref
  #    extra - extra channels array ref (optional)
  #  Expected results:
  #    inchans - number of input channels
  #    outchans - number of color output channels
  #    extrachans - number of extra output channels (optional, default 0)
  #    coeff - resulting coefficient array
  #    message - qr// expected to match an error if any
  my @tests =
    (
      {
        name => "Simple 1 to 1 channel",
        main => [ [ 1 ] ],
        inchans => 1,
        outchans => 1,
        coeff => [ 1 ],
      },
      {
        name => "2 channel identity",
        main => [ [ 1, 0 ], [ 0, 1 ] ],
        inchans => 2,
        outchans => 2,
        coeff => [ 1, 0, 0, 1 ],
      },
      {
        name => "3 to 1 (RGB to gray)",
        main => [ [ 0.4, 0.3, 0.3 ] ],
        inchans => 3,
        outchans => 1,
        coeff => [ 0.4, 0.3, 0.3 ],
      },
      {
        name => "add alpha to gray",
        main => [ [ 1 ], [ 0, 1 ] ],
        inchans => 2,
        outchans => 2,
        coeff => [ 1, 0, 0, 1 ]
      },
      {
        name => "rgb to grey and add alpha",
        main => [ [ 0.3, 0.3, 0.3 ], [ 0, 0, 0, 1 ] ],
        inchans => 4,
        outchans => 2,
        coeff => [ 0.3, 0.3, 0.3, 0, 0, 0, 0, 1 ],
      },
      {
        name => "add 1.0 extra to gray",
        main => [ [ 1 ] ],
        extra => [ [ 0, 1 ] ],
        inchans => 2,
        outchans => 1,
        extrachans => 1,
        coeff => [ 1, 0, 0, 1 ],
      },
      {
        name => "add 2 extra channels, of values 0 and 1 to gray",
        main => [ [ 1 ] ],
        extra => [ [ 0, 0 ], [ 0, 1 ] ],
        inchans => 2,
        outchans => 1,
        extrachans => 2,
        coeff => [ 1, 0, 0, 0, 0, 1 ],
      },
      {
        name => "bad base matrix",
        main => [ 0 ],
        message => qr/invalid matrix: element 0 is not an array ref/
      },
      {
        name => "bad base matrix second element",
        main => [ [ 0 ], 0 ],
        message => qr/invalid matrix: element 1 is not an array ref/
      },
      {
        name => "bad extra matrix",
        main => [ [ 0 ] ],
        extra => [ [ 0 ], 1 ],
        message => qr/invalid extra matrix: element 1 is not an array ref/
      },
     );

  for my $test (@tests) {
    my $name = $test->{name};
    my ($inchans, $outchans, $extrachans, $coeff);
    if ($test->{extra}) {
      ($inchans, $outchans, $extrachans, $coeff) =
        Imager::InternalTest::extract_convert_matrix($test->{main}, $test->{extra});
    }
    else {
      ($inchans, $outchans, $extrachans, $coeff) =
        Imager::InternalTest::extract_convert_matrix($test->{main});
    }
    if ($test->{coeff}) {
      is_arrayf($coeff, $test->{coeff}, "$name: coefficients");
      is($inchans, $test->{inchans}, "$name: input channels");
      is($outchans, $test->{outchans}, "$name: output channels");
      is($extrachans, $test->{extrachans} || 0, "$name: extra channels");
    }
    else {
      is($coeff, undef, "$name: expected to fail");
      my $msg = Imager->_error_as_msg;
      like($msg, $test->{message}, "$name: check message");
    }
  }
}

my $imbase = Imager::ImgRaw::new(200,300,3);

# first a basic test, make sure the basic things happen ok
# make a 1 channel image from the above (black) image
# but with 1 as the 'extra' value
SKIP:
{
  my $im_white = i_convert($imbase, [ [ 0, 0, 0, 1 ] ], []);
  skip("convert to white failed", 3)
    unless ok($im_white, "convert to white");

  my ($w, $h, $ch) = i_img_info($im_white);

  # the output image should now have one channel
  is($ch, 1, "one channel image now");
  # should have the same width and height
  ok($w == 200 && $h == 300, "check converted size is the same");

  # should be a white image now, let's check
  my $c = Imager::i_get_pixel($im_white, 20, 20);
  my @c = $c->rgba;
  print "# @c\n";
  is($c[0], 255, "check image is white");
}

{
  # basic extra channels test
  my $im = Imager::ImgRaw::new(20, 20, 3);
  Imager::i_ppix($im, 0, 0, NC(255, 0, 0));
  my @osamp = Imager::i_gsamp($im, 0, 1, 0, 3);
  is_deeply(\@osamp, [ 255, 0, 0 ], "check written samples");
  my $extraim = Imager::i_convert($im, [], [ [ 0.5, 0.5, 0 ], [ 0, 0, 0, 1 ] ]);
  ok($extraim, "make extra channels image");
  is(Imager::i_img_getchannels($extraim), 0, "had no normal channels");
  is(Imager::i_img_extra_channels($extraim), 2, "had 2 extra channels");
  my @samp = Imager::i_gsamp($extraim, 0, 1, 0, 2);
  is_deeply(\@samp, [ 127, 255 ], "check samples expected")
    or diag "@samp";

  # test handling of copy-only
  my $extraim2 = Imager::i_convert($im, [], [ [ 1 ] ]);
  ok($extraim2, "got copy only extra channel image")
    or diag(Imager->_error_as_msg);
  is(Imager::i_img_getchannels($extraim2), 0, "copy only: no normal channels");
  is(Imager::i_img_extra_channels($extraim2), 1, "copy only: 1 extra channel");
  @samp = Imager::i_gsamp($extraim2, 0, 1, 0, 1);
  is_deeply(\@samp, [ 255 ], "copy only: check samples expected");
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
{
 SKIP:
  {
    my $imbase16 = Imager::i_img_16_new(200, 200, 3);

    my $im16targ = i_convert($imbase16,
                             [ [ 0, 0, 0, 1 ],
                               [ 0, 0, 0, 0 ],
                               [ 0, 0, 0, 0 ] ],
                             []);
    ok($im16targ, "convert 16/bit sample image")
      or skip("could not convert 16-bit image", 2);

    # image should still be 16-bit
    is(Imager::i_img_bits($im16targ), 16, "Image still 16-bit/sample");

    # make sure that it's roughly red
    test_colorf_gpix($im16targ, 0, 0, NCF(1, 0, 0), 0.001, "image roughly red");
  }
 SKIP:
  {
    my $imbase16 = Imager->new(xsize => 10, ysize => 10, bits => 16);
    ok($imbase16->setpixel
       (x => 5, y => 2, color => Imager::Color::Float->new(0.1, 0.2, 0.3)),
       "set a sample pixel");
    my $c1 = $imbase16->getpixel(x => 5, y => 2, type => "float");
    is_fcolor3($c1, 0.1, 0.2, 0.3, "check it was set")
      or print "#", join(",", $c1->rgba), "\n";
    
    my $targ16 = $imbase16->convert(matrix => [ [ 0.05, 0.15, 0.01, 0.5 ] ], scale => "gamma");
    ok($targ16, "convert another 16/bit sample image")
      or skip("could not convert", 3);
    is($targ16->getchannels, 1, "convert should be 1 channel");
    is($targ16->bits, 16, "and 16-bits");
    my $c = $targ16->getpixel(x => 5, y => 2, type => "float");
    is_fcolor1($c, 0.538, 1/32768, "check grey value");
  }
}

# test against palette based images
my $impal = Imager::i_img_pal_new(200, 300, 3, 256);
my $black = NC(0, 0, 0);
my $blackindex = Imager::i_addcolors($impal, $black);
ok($blackindex, "add black to paletted");
for my $y (0..299) {
  Imager::i_ppal($impal, 0, $y, ($blackindex) x 200);
}

SKIP:
{
  my $impalout = i_convert($impal,
                           [ [ 0, 0, 0, 0 ],
                             [ 0, 0, 0, 1 ],
                             [ 0, 0, 0, 0 ] ],
                          []);
  skip("could not convert paletted", 3)
    unless ok($impalout, "convert paletted");
  is(Imager::i_img_type($impalout), 1, "image still paletted");
  is(Imager::i_colorcount($impalout), 1, "still only one colour");
  my $c = Imager::i_getcolors($impalout, $blackindex);
  ok($c, "get color from palette");
  my @ch = $c->rgba;
  print "# @ch\n";
  ok($ch[0] == 0 && $ch[1] == 255 && $ch[2] == 0, 
     "colour is as expected");
}

{
  # extra images with paletted (which doesn't support extra channels)
  my $impalextra = i_convert($impal,
                             [ [ 0.3, 0.3, 0.3 ] ],
                             [ [ 0,   0,   0,   0.5 ] ]);
  ok($impalextra, "convert paletted image to add extra channels");
  is(Imager::i_img_getchannels($impalextra), 1, "expected base channels");
  is(Imager::i_img_extra_channels($impalextra), 1, "expected extra channels");
  my @samp = Imager::i_gsamp($impalextra, 0, 1, 0, 2);
  is_deeply(\@samp, [ 0, 127 ], "check extra channels set");
}

{ # http://rt.cpan.org/NoAuth/Bug.html?id=9672
  # methods that return a new image should warn in void context
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
  cmp_ok($warning, '=~', 'convert\\.t', "correct file");
}

{ # http://rt.cpan.org/NoAuth/Bug.html?id=28492
  # convert() doesn't preserve image sample size
  my $im = Imager->new(xsize => 20, ysize => 20, channels => 3, 
		       bits => 'double');
  is($im->bits, 'double', 'check source bits');
  my $conv = $im->convert(preset => 'grey');
  is($conv->bits, 'double', 'make sure result has extra bits');
}

{ # http://rt.cpan.org/NoAuth/Bug.html?id=79922
  # Segfault in convert with bad params
  my $im = Imager->new(xsize => 10, ysize => 10);
  ok(!$im->convert(matrix => [ 10, 10, 10 ]),
     "this would crash");
  is($im->errstr, "convert: invalid matrix: element 0 is not an array ref",
     "check the error message");
  my $im2 = $im->convert(preset => "grey", extra => [ [ 0, 0, 0, 3 ] ]);
  ok($im2, "convert OO with extras");
  is($im2->getchannels, 1, "expected channels");
  is($im2->extrachannels, 1, "expected extra channels");
}

{
  my $im = Imager->new(xsize => 10, ysize => 10, channels => 3, extrachannels => 1);
  is($im->setsamples(y => 0, data => [ 127, 64, 255, 127 ], channels => 4), 4,
     "write some samples");
  is_deeply([ $im->getsamples(y => 0, width => 1, channels => 4) ],
            [ 127, 64, 255, 127 ],
            "check samples");
}

{
  my $empty = Imager->new;
  ok(!$empty->convert(preset => "addalpha"), "can't convert an empty image");
  is($empty->errstr, "convert: empty input image", "check error message");
}

{
  my $im = test_image;
  ok(!$im->convert(matrix => [ map { [ 0 ] } 1 .. 5 ]),
     "can't convert to image with 5 normal channels");
  like($im->errstr, qr/convert: cannot have outchans \(5\) > MAXCHANNELS \(4\)/,
       "check message");
  ok(!$im->convert(matrix => [  ], extra => [  ]),
     "can't convert to image with no total channels");
  like($im->errstr, qr/convert: there must be at least one output channel/,
       "check message");
  ok(!$im->convert(matrix => [ ([0]) x 4 ], extra => [ ( [0] ) x 11 ]),
     "can't convert to image with too many total channels");
  like($im->errstr, qr/convert: cannot have normal output channels \(4\) \+ extra channels \(11\) > maximum total channels \(14\)/,
       "check message");
}

sub limit8 {
  my $d = shift;
  if ($d < 0) {
    return 0;
  }
  elsif ($d > 255) {
    return 255;
  }
  else {
    return $d;
  }
}

sub limitd {
  my $d = shift;
  if ($d < 0) {
    return 0;
  }
  elsif ($d > 1.0) {
    return 1.0;
  }
  else {
    return $d;
  }
}

{
  # gamma
  my @samps = ( 255, 127, 64 );
  my $im = Imager->new(xsize => 10, ysize => 10, channels => 1);
  is($im->setsamples(y => 1, data => \@samps), 3, "set some samples");

  {
    my $result = $im->convert(matrix => [ [ 0.5 ] ], scale => "gamma");
    ok($result, "make 0.5 input gamma converted image");
    is_deeply([ $result->getsamples(y => 1, width => 3) ], [ map { int($_/2) } @samps ],
              "check samples scaled as if gamma was luminance");
  }
  {
    my $result = $im->convert(matrix => [ [ 0.5 ] ]);
    ok($result, "make 0.5 input linear converted image");
    my @expect = map { sprintf("%.0f", to_gamma_srgb(to_linear_srgb($_)/2)) } @samps;
    is_deeply([ $result->getsamples(y => 1, width => 3) ], \@expect,
              "check samples scaled by luminance");
  }
  {
    my $result = $im->convert(matrix => [ [ 1.0, -0.5 ] ], scale => "gamma");
    ok($result, "make (+1.0, -0.5) gamma converted image");
    my @expect = map { limit8(int($_ - (255/2))) } @samps;
    is_deeply([ $result->getsamples(y => 1, width => 3) ],
              \@expect,
              "check with 1.0 of input sample (gamma)");
  }
  {
    my $result = $im->convert(matrix => [ [ 1.0, -0.5 ] ],);
    ok($result, "make (+1.0, -0.5) gamma converted image");
    my @expect = map { sprintf("%.0f", limit8(to_gamma_srgb(to_linear_srgb($_) - (65535/2)))) } @samps;
    is_deeply([ $result->getsamples(y => 1, width => 3) ],
              \@expect,
              "check with 1.0 of input sample");
  }

  # similar for float
  my $fim = $im->to_rgb_double;
  my @fsamps = map { $_ / 255 } @samps;
  {
    my $result = $fim->convert(matrix => [ [ 0.5 ] ], scale => "gamma");
    ok($result, "float: make 0.5 input gamma converted image");
    is_arrayf([ $result->getsamples(y => 1, width => 3, type => "float") ],
              [ map { $_/2 } @fsamps ],
              "check samples scaled as if gamma was luminance");
  }
  {
    my $result = $fim->convert(matrix => [ [ 0.5 ] ]);
    ok($result, "float: make 0.5 input linear converted image");
    my @expect = map { to_gamma_srgbf(to_linear_srgbf($_)/2) } @fsamps;
    is_arrayf([ $result->getsamples(y => 1, width => 3, type => "float") ], \@expect,
              "check samples scaled by luminance");
  }
  {
    my $result = $fim->convert(matrix => [ [ 1.0, -0.5 ] ], scale => "gamma");
    ok($result, "float: make (+1.0, -0.5) gamma converted image");
    my @expect = map { limitd($_ - 0.5) } @fsamps;
    is_arrayf([ $result->getsamples(y => 1, width => 3, type => "float") ],
              \@expect,
              "float: check with 1.0 of input sample (gamma)");
  }
  {
    my $result = $fim->convert(matrix => [ [ 1.0, -0.5 ] ]);
    ok($result, "float: make (+1.0, -0.5) gamma converted image");
    my @expect = map { limitd(to_gamma_srgbf(to_linear_srgbf($_) - 0.5)) } @fsamps;
    is_arrayf([ $result->getsamples(y => 1, width => 3, type => "float") ],
              \@expect,
              "float: check with 1.0 of input sample");
  }

}

done_testing();
