#!perl -w
# t/t01introvert.t - tests internals of image formats
# to make sure we get expected values

use strict;
use Test::More tests=>196;

BEGIN { use_ok(Imager => qw(:handy :all)) }

require "t/testtools.pl";

init_log("testout/t01introvert.log",1);

my $im_g = Imager::ImgRaw::new(100, 101, 1);

my $red = NC(255, 0, 0);
my $green = NC(0, 255, 0);
my $blue = NC(0, 0, 255);

use Imager::Color::Float;
my $f_black = Imager::Color::Float->new(0, 0, 0);
my $f_red = Imager::Color::Float->new(1.0, 0, 0);
my $f_green = Imager::Color::Float->new(0, 1.0, 0);
my $f_blue = Imager::Color::Float->new(0, 0, 1.0);

is(Imager::i_img_getchannels($im_g), 1, "1 channel image channel count");
ok(Imager::i_img_getmask($im_g) & 1, "1 channel image mask");
ok(!Imager::i_img_virtual($im_g), "1 channel image not virtual");
is(Imager::i_img_bits($im_g), 8, "1 channel image has 8 bits/sample");
is(Imager::i_img_type($im_g), 0, "1 channel image is direct");

my @ginfo = Imager::i_img_info($im_g);
is($ginfo[0], 100, "1 channel image width");
is($ginfo[1], 101, "1 channel image height");

undef $im_g; # can we check for release after this somehow?

my $im_rgb = Imager::ImgRaw::new(100, 101, 3);

is(Imager::i_img_getchannels($im_rgb), 3, "3 channel image channel count");
is((Imager::i_img_getmask($im_rgb) & 7), 7, "3 channel image mask");
is(Imager::i_img_bits($im_rgb), 8, "3 channel image has 8 bits/sample");
is(Imager::i_img_type($im_rgb), 0, "3 channel image is direct");

undef $im_rgb;

my $im_pal = Imager::i_img_pal_new(100, 101, 3, 256);

ok($im_pal, "make paletted image");
is(Imager::i_img_getchannels($im_pal), 3, "pal img channel count");
is(Imager::i_img_bits($im_pal), 8, "pal img bits");
is(Imager::i_img_type($im_pal), 1, "pal img is paletted");

my $red_idx = check_add($im_pal, $red, 0);
my $green_idx = check_add($im_pal, $green, 1);
my $blue_idx = check_add($im_pal, $blue, 2);

# basic writing of palette indicies
# fill with red
is(Imager::i_ppal($im_pal, 0, 0, ($red_idx) x 100), 100, 
   "write red 100 times");
# and blue
is(Imager::i_ppal($im_pal, 50, 0, ($blue_idx) x 50), 50,
   "write blue 50 times");

# make sure we get it back
my @pals = Imager::i_gpal($im_pal, 0, 100, 0);
ok(!grep($_ != $red_idx, @pals[0..49]), "check for red");
ok(!grep($_ != $blue_idx, @pals[50..99]), "check for blue");
is(Imager::i_gpal($im_pal, 0, 100, 0), "\0" x 50 . "\2" x 50, 
   "gpal in scalar context");
my @samp = Imager::i_gsamp($im_pal, 0, 100, 0, 0, 1, 2);
is(@samp, 300, "gsamp count in list context");
my @samp_exp = ((255, 0, 0) x 50, (0, 0, 255) x 50);
is_deeply(\@samp, \@samp_exp, "gsamp list deep compare");
my $samp = Imager::i_gsamp($im_pal, 0, 100, 0, 0, 1, 2);
is(length($samp), 300, "gsamp scalar length");
is($samp, "\xFF\0\0" x 50 . "\0\0\xFF" x 50, "gsamp scalar bytes");

# reading indicies as colors
my $c_red = Imager::i_get_pixel($im_pal, 0, 0);
ok($c_red, "got the red pixel");
ok(color_cmp($red, $c_red) == 0, "and it's red");
my $c_blue = Imager::i_get_pixel($im_pal, 50, 0);
ok($c_blue, "got the blue pixel");
ok(color_cmp($blue, $c_blue) == 0, "and it's blue");

# drawing with colors
ok(Imager::i_ppix($im_pal, 0, 0, $green) == 0, "draw with color in palette");
# that was in the palette, should still be paletted
is(Imager::i_img_type($im_pal), 1, "image still paletted");

my $c_green = Imager::i_get_pixel($im_pal, 0, 0);
ok($c_green, "got green pixel");
ok(color_cmp($green, $c_green) == 0, "and it's green");

is(Imager::i_colorcount($im_pal), 3, "still 3 colors in palette");
is(Imager::i_findcolor($im_pal, $green), 1, "and green is the second");

my $black = NC(0, 0, 0);
# this should convert the image to RGB
ok(Imager::i_ppix($im_pal, 1, 0, $black) == 0, "draw with black (not in palette)");
is(Imager::i_img_type($im_pal), 0, "pal img shouldn't be paletted now");

my %quant =
  (
   colors => [$red, $green, $blue, $black],
   makemap => 'none',
  );
my $im_pal2 = Imager::i_img_to_pal($im_pal, \%quant);
ok($im_pal2, "got an image from quantizing");
is(@{$quant{colors}}, 4, "has the right number of colours");
is(Imager::i_gsamp($im_pal2, 0, 100, 0, 0, 1, 2),
  "\0\xFF\0\0\0\0"."\xFF\0\0" x 48 . "\0\0\xFF" x 50,
   "colors are still correct");

# test the OO interfaces
my $impal2 = Imager->new(type=>'pseudo', xsize=>200, ysize=>201);
ok($impal2, "make paletted via OO");
is($impal2->getchannels, 3, "check channels");
is($impal2->bits, 8, "check bits");
is($impal2->type, 'paletted', "check type");

{
  my $red_idx = $impal2->addcolors(colors=>[$red]);
  ok($red_idx, "add red to OO");
  is(0+$red_idx, 0, "and it's expected index for red");
  my $blue_idx = $impal2->addcolors(colors=>[$blue, $green]);
  ok($blue_idx, "add blue/green via OO");
  is($blue_idx, 1, "and it's expected index for blue");
  my $green_idx = $blue_idx + 1;
  my $c = $impal2->getcolors(start=>$green_idx);
  ok(color_cmp($green, $c) == 0, "found green where expected");
  my @cols = $impal2->getcolors;
  is(@cols, 3, "got 3 colors");
  my @exp = ( $red, $blue, $green );
  my $good = 1;
  for my $i (0..2) {
    if (color_cmp($cols[$i], $exp[$i])) {
      $good = 0;
      last;
    }
  }
  ok($good, "all colors in palette as expected");
  is($impal2->colorcount, 3, "and colorcount returns 3");
  is($impal2->maxcolors, 256, "maxcolors as expected");
  is($impal2->findcolor(color=>$blue), 1, "findcolors found blue");
  ok($impal2->setcolors(start=>0, colors=>[ $blue, $red ]),
     "we can setcolors");

  # make an rgb version
  my $imrgb2 = $impal2->to_rgb8();
  is($imrgb2->type, 'direct', "converted is direct");

  # and back again, specifying the palette
  my @colors = ( $red, $blue, $green );
  my $impal3 = $imrgb2->to_paletted(colors=>\@colors,
                                    make_colors=>'none',
                                    translate=>'closest');
  ok($impal3, "got a paletted image from conversion");
  dump_colors(@colors);
  print "# in image\n";
  dump_colors($impal3->getcolors);
  is($impal3->colorcount, 3, "new image has expected color table size");
  is($impal3->type, 'paletted', "and is paletted");
}

ok(!Imager->new(xsize=>0, ysize=>1), "fail to create 0 height image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "0 height error message check");
ok(!Imager->new(xsize=>1, ysize=>0), "fail to create 0 width image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "0 width error message check");
ok(!Imager->new(xsize=>-1, ysize=>1), "fail to create -ve height image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "-ve width error message check");
ok(!Imager->new(xsize=>1, ysize=>-1), "fail to create -ve width image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "-ve height error message check");
ok(!Imager->new(xsize=>-1, ysize=>-1), "fail to create -ve width/height image");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "-ve width/height error message check");

ok(!Imager->new(xsize=>1, ysize=>1, channels=>0),
   "fail to create a zero channel image");
cmp_ok(Imager->errstr, '=~', qr/channels must be between 1 and 4/,
       "out of range channel message check");
ok(!Imager->new(xsize=>1, ysize=>1, channels=>5),
   "fail to create a five channel image");
cmp_ok(Imager->errstr, '=~', qr/channels must be between 1 and 4/,
       "out of range channel message check");

{
  # https://rt.cpan.org/Ticket/Display.html?id=8213
  # check for handling of memory allocation of very large images
  # only test this on 32-bit machines - on a 64-bit machine it may
  # result in trying to allocate 4Gb of memory, which is unfriendly at
  # least and may result in running out of memory, causing a different
  # type of exit
 SKIP:
  {
    use Config;
    skip("don't want to allocate 4Gb", 8) unless $Config{intsize} == 4;

    my $uint_range = 256 ** $Config{intsize};
    print "# range $uint_range\n";
    my $dim1 = int(sqrt($uint_range))+1;
    
    my $im_b = Imager->new(xsize=>$dim1, ysize=>$dim1, channels=>1);
    is($im_b, undef, "integer overflow check - 1 channel");
    
    $im_b = Imager->new(xisze=>$dim1, ysize=>1, channels=>1);
    ok($im_b, "but same width ok");
    $im_b = Imager->new(xisze=>1, ysize=>$dim1, channels=>1);
    ok($im_b, "but same height ok");
    cmp_ok(Imager->errstr, '=~', qr/integer overflow/,
           "check the error message");

    # do a similar test with a 3 channel image, so we're sure we catch
    # the same case where the third dimension causes the overflow
    my $dim3 = int(sqrt($uint_range / 3))+1;
    
    $im_b = Imager->new(xsize=>$dim3, ysize=>$dim3, channels=>3);
    is($im_b, undef, "integer overflow check - 3 channel");
    
    $im_b = Imager->new(xisze=>$dim3, ysize=>1, channels=>3);
    ok($im_b, "but same width ok");
    $im_b = Imager->new(xisze=>1, ysize=>$dim3, channels=>3);
    ok($im_b, "but same height ok");

    cmp_ok(Imager->errstr, '=~', qr/integer overflow/,
           "check the error message");
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
  $img->to_rgb8(); # doesn't really matter what the source is
  cmp_ok($warning, '=~', 'void', "correct warning");
  cmp_ok($warning, '=~', 't01introvert\\.t', "correct file");
}

{ # http://rt.cpan.org/NoAuth/Bug.html?id=11860
  my $im = Imager->new(xsize=>2, ysize=>2);
  $im->setpixel(x=>0, 'y'=>0, color=>$red);
  $im->setpixel(x=>1, 'y'=>0, color=>$blue);

  my @row = Imager::i_glin($im->{IMG}, 0, 2, 0);
  is(@row, 2, "got 2 pixels from i_glin");
  ok(color_cmp($row[0], $red) == 0, "red first");
  ok(color_cmp($row[1], $blue) == 0, "then blue");
}

{ # general tag tests
  
  # we don't care much about the image itself
  my $im = Imager::ImgRaw::new(10, 10, 1);

  ok(Imager::i_tags_addn($im, 'alpha', 0, 101), "i_tags_addn(...alpha, 0, 101)");
  ok(Imager::i_tags_addn($im, undef, 99, 102), "i_tags_addn(...undef, 99, 102)");
  is(Imager::i_tags_count($im), 2, "should have 2 tags");
  ok(Imager::i_tags_addn($im, undef, 99, 103), "i_tags_addn(...undef, 99, 103)");
  is(Imager::i_tags_count($im), 3, "should have 3 tags, despite the dupe");
  is(Imager::i_tags_find($im, 'alpha', 0), '0 but true', "find alpha");
  is(Imager::i_tags_findn($im, 99, 0), 1, "find 99");
  is(Imager::i_tags_findn($im, 99, 2), 2, "find 99 again");
  is(Imager::i_tags_get($im, 0), 101, "check first");
  is(Imager::i_tags_get($im, 1), 102, "check second");
  is(Imager::i_tags_get($im, 2), 103, "check third");

  ok(Imager::i_tags_add($im, 'beta', 0, "hello", 0), 
     "add string with string key");
  ok(Imager::i_tags_add($im, 'gamma', 0, "goodbye", 0),
     "add another one");
  ok(Imager::i_tags_add($im, undef, 199, "aloha", 0),
     "add one keyed by number");
  is(Imager::i_tags_find($im, 'beta', 0), 3, "find beta");
  is(Imager::i_tags_find($im, 'gamma', 0), 4, "find gamma");
  is(Imager::i_tags_findn($im, 199, 0), 5, "find 199");
  ok(Imager::i_tags_delete($im, 2), "delete");
  is(Imager::i_tags_find($im, 'beta', 0), 2, 'find beta after deletion');
  ok(Imager::i_tags_delbyname($im, 'beta'), 'delete beta by name');
  is(Imager::i_tags_find($im, 'beta', 0), undef, 'beta not there now');
  is(Imager::i_tags_get_string($im, "gamma"), "goodbye", 
     'i_tags_get_string() on a string');
  is(Imager::i_tags_get_string($im, 99), 102, 
     'i_tags_get_string() on a number entry');
  ok(Imager::i_tags_delbycode($im, 99), 'delete by code');
  is(Imager::i_tags_findn($im, 99, 0), undef, '99 not there now');
  is(Imager::i_tags_count($im), 3, 'final count of 3');
}

{ 
  print "# low-level scan line function tests\n";
  my $im = Imager::ImgRaw::new(10, 10, 4);
  Imager::i_ppix($im, 5, 0, $red);

  # i_glin/i_glinf
  my @colors = Imager::i_glin($im, 0, 10, 0);
  is_deeply([ (0) x 20, (255, 0, 0, 255), (0) x 16 ], 
	    [ map $_->rgba, @colors ],
	    "i_glin - list context");
  my $colors = Imager::i_glin($im, 0, 10, 0);
  is("00" x 20 . "FF0000FF" . "00" x 16, 
     uc unpack("H*", $colors), "i_glin - scalar context");
  my @fcolors = Imager::i_glinf($im, 0, 10, 0);
  is_deeply([ (0.0) x 20, (1.0, 0, 0, 1.0) , (0) x 16 ],
	    [ map $_->rgba, @fcolors ],
	    "i_glinf - list context");
  my $fcolors = Imager::i_glinf($im, 0, 10, 0);
  is_deeply([ (0.0) x 20, (1.0, 0, 0, 1.0) , (0) x 16 ],
	    [ unpack "d*", $fcolors ],
	    "i_glinf - scalar context");

  # i_plin/i_plinf
  my @plin_colors = (($black) x 4, $red, $blue, ($black) x 4);
  is(Imager::i_plin($im, 0, 1, @plin_colors),
     10, "i_plin - pass in a list");
  # make sure we get it back
  is_deeply([ map [ $_->rgba ], @plin_colors ],
	    [ map [ $_->rgba ], Imager::i_glin($im, 0, 10, 1) ],
	    "check i_plin wrote to the image");
  my @scalar_plin = 
    (
     (0,0,0,0) x 4, 
     (0, 255, 0, 255),
     (0, 0, 255, 255), 
     (0, 0, 0, 0) x 4,
    );
  is(Imager::i_plin($im, 0, 2, pack("C*", @scalar_plin)),
     10, "i_plin - pass in a scalar");
  is_deeply(\@scalar_plin,
	    [ map $_->rgba , Imager::i_glin($im, 0, 10, 2) ],
	    "check i_plin scalar wrote to the image");

  my @plinf_colors = # Note: only 9 pixels
    ( 
     ($f_blue) x 4, 
     $f_red, 
     ($f_black) x 3, 
     $f_black
    );
  is(Imager::i_plinf($im, 0, 3, @plinf_colors), 9,
     "i_plinf - list");
  is_deeply([ map $_->rgba, Imager::i_glinf($im, 0, 9, 3) ],
	    [ map $_->rgba, @plinf_colors ],
	    "check colors were written");
  my @scalar_plinf =
    (
     ( 1.0, 1.0,   0, 1.0 ) x 3,
     (   0, 1.0, 1.0, 1.0 ) x 2,
     (   0,   0,   0,   0 ),
     ( 1.0,   0, 1.0, 1.0 ),
    );
  is(Imager::i_plinf($im, 2, 4, pack("d*", @scalar_plinf)),
     7, "i_plinf - scalar");
  is_deeply(\@scalar_plinf,
	    [ map $_->rgba, Imager::i_glinf($im, 2, 9, 4) ],
	    "check colors were written");

  is_deeply([ Imager::i_gsamp($im, 0, 10, 0, (0, 3)) ],
	    [ (0, 0) x 5, (255, 255), (0, 0) x 4 ],
	    "i_gsamp list context");
  is("0000" x 5 . "FFFF" . "0000" x 4,
     uc unpack("H*", Imager::i_gsamp($im, 0, 10, 0, (0, 3))),
     "i_gsamp scalar context");
  is_deeply([ Imager::i_gsampf($im, 2, 9, 4, 0, 2, 3) ],
	    [ (1.0, 0, 1.0) x 3, (0, 1.0, 1.0) x 2, (0, 0, 0),
	      (1.0, 1.0, 1.0) ], "i_gsampf - list context");
  is_deeply([ unpack("d*", Imager::i_gsampf($im, 2, 9, 4, 0, 2, 3)) ],
	    [ (1.0, 0, 1.0) x 3, (0, 1.0, 1.0) x 2, (0, 0, 0),
              (1.0, 1.0, 1.0) ], "i_gsampf - scalar context");
  print "# end low-level scan-line function tests\n";
}

{
  print "# OO level scanline function tests\n";
  my $im = Imager->new(xsize=>10, ysize=>10, channels=>4);
  $im->setpixel(color=>$red, 'x'=>5, 'y'=>0);
  ok(!$im->getscanline(), "getscanline() - supply nothing, get nothing");
  is($im->errstr, "missing y parameter", "check message");
  is_deeply([ map [ $_->rgba ], $im->getscanline('y'=>0) ],
	    [ ([ 0,0,0,0]) x 5, [ 255, 0, 0, 255 ], ([ 0,0,0,0]) x 4 ],
	    "getscanline, list context, default x, width");
  is_deeply([ map [ $_->rgba ], $im->getscanline('y'=>0, 'x'=>3) ],
	    [ ([0,0,0,0]) x 2, [ 255, 0, 0, 255 ], ([0,0,0,0]) x 4 ],
	    "getscanline, list context, default width");
  is_deeply([ map [ $_->rgba ], $im->getscanline('y'=>0, 'x'=>4, width=>4) ],
	    [ [0,0,0,0], [ 255, 0, 0, 255 ], ([0,0,0,0]) x 2 ],
	    "getscanline, list context, no defaults");
  is(uc unpack("H*",  $im->getscanline('y'=>0)),
     "00000000" x 5 .  "FF0000FF" . "00000000" x 4,
     "getscanline, scalar context, default x, width");
  is_deeply([ map [ $_->rgba ], 
	      $im->getscanline('y'=>0, 'x'=>4, width=>4, type=>'float') ],
	    [ [0,0,0,0], [ 1.0, 0, 0, 1.0 ], ([0,0,0,0]) x 2 ],
	    "getscanline float, list context, no defaults");
  is_deeply([ unpack "d*",
	      $im->getscanline('y'=>0, 'x'=>4, width=>4, type=>'float') ],
	    [ (0,0,0,0), ( 1.0, 0, 0, 1.0 ), (0,0,0,0) x 2 ],
	    "getscanline float, scalar context, no defaults");

  ok(!$im->getscanline('y'=>0, type=>'invalid'),
     "check invalid type checking");
  like($im->errstr, qr/invalid type parameter/, 
       "check message for invalid type");

  my @plin_colors = (($black) x 4, $red, $blue, ($green) x 4);
  is($im->setscanline('y'=>1, pixels=>\@plin_colors), 10,
     "setscanline - arrayref, default x");
  is_deeply([ map [ $_->rgba ], @plin_colors ],
	    [ map [ $_->rgba ], $im->getscanline('y'=>1) ],
	    "check colors were written");

  my @plin_colors2 = ( $green, $red, $blue, $red );
  is($im->setscanline('y'=>2, 'x'=>3, pixels=>\@plin_colors2), 4,
     "setscanline - arrayref");

  # using map instead of x here due to a bug in some versions of Test::More
  # fixed in the latest Test::More
  is_deeply([ ( map [ 0,0,0,0 ], 1..3), (map [ $_->rgba ], @plin_colors2),
	      ( map [ 0,0,0,0 ], 1..3) ],
	    [ map [ $_->rgba ], $im->getscanline('y'=>2) ],
	    "check write to middle of line");
  
  my $raw_colors = pack "H*", "FF00FFFF"."FF0000FF"."FFFFFFFF";
  is($im->setscanline('y'=>3, 'x'=>2, pixels=>$raw_colors), 3,
     "setscanline - scalar, default raw type")
    or print "# ",$im->errstr,"\n";
  is(uc unpack("H*", $im->getscanline('y'=>3, 'x'=>1, 'width'=>5)),
     "00000000".uc(unpack "H*", $raw_colors)."00000000",
     "check write");

  # float colors
  my @fcolors = ( $f_red, $f_blue, $f_black, $f_green );
  is($im->setscanline('y'=>4, 'x'=>3, pixels=>\@fcolors), 4,
     "setscanline - float arrayref");
  is_deeply([ map [ $_->rgba ], @fcolors ],
	    [ map [ $_->rgba ], $im->getscanline('y'=>4, 'x'=>3, width=>4, type=>'float') ],
	    "check write");
  # packed
  my $packed_fcolors = pack "d*", map $_->rgba, @fcolors;
  is($im->setscanline('y'=>5, 'x'=>4, pixels=>$packed_fcolors, type=>'float'), 4,
     "setscanline - float scalar");
  is_deeply([ map [ $_->rgba ], @fcolors ],
	    [ map [ $_->rgba ], $im->getscanline('y'=>5, 'x'=>4, width=>4, type=>'float') ],
	    "check write");

  # get samples
  is_deeply([ $im->getsamples('y'=>1, channels=>[ 0 ]) ],
	    [ map +($_->rgba)[0], @plin_colors ],
	    "get channel 0, list context, default x, width");
  is_deeply([ unpack "C*", $im->getsamples('y'=>1, channels=>[0, 2]) ],
	    [ map { ($_->rgba)[0, 2] } @plin_colors ],
	    "get channel 0, 1, scalar context");
  is_deeply([ $im->getsamples('y'=>4, 'x'=>3, width=>4, type=>'float',
			      channels=>[1,3]) ],
	    [ map { ($_->rgba)[1,3] } @fcolors ],
	    "get channels 1,3, list context, float samples");
  is_deeply([ unpack "d*", 
	      $im->getsamples('y'=>4, 'x'=>3, width=>4,
			      type=>'float', channels=>[3,2,1,0]) ],
	    [ map { ($_->rgba)[3,2,1,0] } @fcolors ],
	    "get channels 3..0 as scalar, float samples");
  
  print "# end OO level scanline function tests\n";
}

{ # check the channel mask function
  
  my $im = Imager->new(xsize => 10, ysize=>10, bits=>8);

  mask_tests($im, 0.005);
}

sub check_add {
  my ($im, $color, $expected) = @_;
  my $index = Imager::i_addcolors($im, $color);
  ok($index, "got index");
  print "# $index\n";
  is(0+$index, $expected, "index matched expected");
  my ($new) = Imager::i_getcolors($im, $index);
  ok($new, "got the color");
  ok(color_cmp($new, $color) == 0, "color matched what was added");

  $index;
}

# sub array_ncmp {
#   my ($a1, $a2) = @_;
#   my $len = @$a1 < @$a2 ? @$a1 : @$a2;
#   for my $i (0..$len-1) {
#     my $diff = $a1->[$i] <=> $a2->[$i] 
#       and return $diff;
#   }
#   return @$a1 <=> @$a2;
# }

sub dump_colors {
  for my $col (@_) {
    print "# ", map(sprintf("%02X", $_), ($col->rgba)[0..2]),"\n";
  }
}
