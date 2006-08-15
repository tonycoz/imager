#!perl -w
# some of this is tested in t01introvert.t too
use strict;
use lib 't';
use Test::More tests => 83;
BEGIN { use_ok("Imager"); }

sub isbin($$$);

my $img = Imager->new(xsize=>50, ysize=>50, type=>'paletted');

ok($img, "paletted image created");

ok($img->type eq 'paletted', "got a paletted image");

my $black = Imager::Color->new(0,0,0);
my $red = Imager::Color->new(255,0,0);
my $green = Imager::Color->new(0,255,0);
my $blue = Imager::Color->new(0,0,255);

my $white = Imager::Color->new(255,255,255);

# add some color
my $blacki = $img->addcolors(colors=>[ $black, $red, $green, $blue ]);

print "# blacki $blacki\n";
ok(defined $blacki && $blacki == 0, "we got the first color");

ok($img->colorcount() == 4, "should have 4 colors");
my ($redi, $greeni, $bluei) = 1..3;

my @all = $img->getcolors;
ok(@all == 4, "all colors is 4");
coloreq($all[0], $black, "first black");
coloreq($all[1], $red, "then red");
coloreq($all[2], $green, "then green");
coloreq($all[3], $blue, "and finally blue");

# keep this as an assignment, checking for scalar context
# we don't want the last color, otherwise if the behaviour changes to
# get all up to the last (count defaulting to size-index) we'd get a
# false positive
my $one_color = $img->getcolors(start=>$redi);
ok($one_color->isa('Imager::Color'), "check scalar context");
coloreq($one_color, $red, "and that it's what we want");

# make sure we can find colors
ok(!defined($img->findcolor(color=>$white)), 
    "shouldn't be able to find white");
ok($img->findcolor(color=>$black) == $blacki, "find black");
ok($img->findcolor(color=>$red) == $redi, "find red");
ok($img->findcolor(color=>$green) == $greeni, "find green");
ok($img->findcolor(color=>$blue) == $bluei, "find blue");

# various failure tests for setcolors
ok(!defined($img->setcolors(start=>-1, colors=>[$white])),
    "expect failure: low index");
ok(!defined($img->setcolors(start=>1, colors=>[])),
    "expect failure: no colors");
ok(!defined($img->setcolors(start=>5, colors=>[$white])),
    "expect failure: high index");

# set the green index to white
ok($img->setcolors(start => $greeni, colors => [$white]),
    "set a color");
# and check it
coloreq(scalar($img->getcolors(start=>$greeni)), $white,
	"make sure it was set");
ok($img->findcolor(color=>$white) == $greeni, "and that we can find it");
ok(!defined($img->findcolor(color=>$green)), "and can't find the old color");

# write a few colors
ok(scalar($img->setcolors(start=>$redi, colors=>[ $green, $red])),
	   "save multiple");
coloreq(scalar($img->getcolors(start=>$redi)), $green, "first of multiple");
coloreq(scalar($img->getcolors(start=>$greeni)), $red, "second of multiple");

# put it back
$img->setcolors(start=>$red, colors=>[$red, $green]);

# draw on the image, make sure it stays paletted when it should
ok($img->box(color=>$red, filled=>1), "fill with red");
ok($img->type eq 'paletted', "paletted after fill");
ok($img->box(color=>$green, filled=>1, xmin=>10, ymin=>10,
	      xmax=>40, ymax=>40), "green box");
ok($img->type eq 'paletted', 'still paletted after box');
# an AA line will almost certainly convert the image to RGB, don't use
# an AA line here
ok($img->line(color=>$blue, x1=>10, y1=>10, x2=>40, y2=>40),
    "draw a line");
ok($img->type eq 'paletted', 'still paletted after line');

# draw with white - should convert to direct
ok($img->box(color=>$white, filled=>1, xmin=>20, ymin=>20, 
	      xmax=>30, ymax=>30), "white box");
ok($img->type eq 'direct', "now it should be direct");

# various attempted to make a paletted image from our now direct image
my $palimg = $img->to_paletted;
ok($palimg, "we got an image");
# they should be the same pixel for pixel
ok(Imager::i_img_diff($img->{IMG}, $palimg->{IMG}) == 0, "same pixels");

# strange case: no color picking, and no colors
# this was causing a segmentation fault
$palimg = $img->to_paletted(colors=>[ ], make_colors=>'none');
ok(!defined $palimg, "to paletted with an empty palette is an error");
print "# ",$img->errstr,"\n";
ok(scalar($img->errstr =~ /no colors available for translation/),
    "and got the correct msg");

ok(!Imager->new(xsize=>1, ysize=>-1, type=>'paletted'), 
    "fail on -ve height");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct error message");
ok(!Imager->new(xsize=>-1, ysize=>1, type=>'paletted'), 
    "fail on -ve width");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct error message");
ok(!Imager->new(xsize=>-1, ysize=>-1, type=>'paletted'), 
    "fail on -ve width/height");
cmp_ok(Imager->errstr, '=~', qr/Image sizes must be positive/,
       "and correct error message");

ok(!Imager->new(xsize=>1, ysize=>1, type=>'paletted', channels=>0),
    "fail on 0 channels");
cmp_ok(Imager->errstr, '=~', qr/Channels must be positive and <= 4/,
       "and correct error message");
ok(!Imager->new(xsize=>1, ysize=>1, type=>'paletted', channels=>5),
    "fail on 5 channels");
cmp_ok(Imager->errstr, '=~', qr/Channels must be positive and <= 4/,
       "and correct error message");

{
  # https://rt.cpan.org/Ticket/Display.html?id=8213
  # check for handling of memory allocation of very large images
  # only test this on 32-bit machines - on a 64-bit machine it may
  # result in trying to allocate 4Gb of memory, which is unfriendly at
  # least and may result in running out of memory, causing a different
  # type of exit
  use Config;
 SKIP:
  {
    skip("don't want to allocate 4Gb", 10)
      unless $Config{intsize} == 4;

    my $uint_range = 256 ** $Config{intsize};
    my $dim1 = int(sqrt($uint_range))+1;
    
    my $im_b = Imager->new(xsize=>$dim1, ysize=>$dim1, channels=>1, type=>'paletted');
    is($im_b, undef, "integer overflow check - 1 channel");
    
    $im_b = Imager->new(xisze=>$dim1, ysize=>1, channels=>1, type=>'paletted');
    ok($im_b, "but same width ok");
    $im_b = Imager->new(xisze=>1, ysize=>$dim1, channels=>1, type=>'paletted');
    ok($im_b, "but same height ok");
    cmp_ok(Imager->errstr, '=~', qr/integer overflow/,
           "check the error message");

    # do a similar test with a 3 channel image, so we're sure we catch
    # the same case where the third dimension causes the overflow
    # for paletted images the third dimension can't cause an overflow
    # but make sure we didn't anything too dumb in the checks
    my $dim3 = $dim1;
    
    $im_b = Imager->new(xsize=>$dim3, ysize=>$dim3, channels=>3, type=>'paletted');
    is($im_b, undef, "integer overflow check - 3 channel");
    
    $im_b = Imager->new(xsize=>$dim3, ysize=>1, channels=>3, type=>'paletted');
    ok($im_b, "but same width ok");
    $im_b = Imager->new(xsize=>1, ysize=>$dim3, channels=>3, type=>'paletted');
    ok($im_b, "but same height ok");

    cmp_ok(Imager->errstr, '=~', qr/integer overflow/,
           "check the error message");

    # test the scanline allocation check
    # divide by 2 to get int range, by 3 so that the image (one byte/pixel)
    # doesn't integer overflow, but the scanline of i_color (4/pixel) does
    my $dim4 = $uint_range / 2 / 3;
    my $im_o = Imager->new(xsize=>$dim4, ysize=>1, channels=>3, type=>'paletted');
    is($im_o, undef, "integer overflow check - scanline size");
    cmp_ok(Imager->errstr, '=~', 
           qr/integer overflow calculating scanline allocation/,
           "check error message");
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
  $img->to_paletted();
  cmp_ok($warning, '=~', 'void', "correct warning");
  cmp_ok($warning, '=~', 't023palette\\.t', "correct file");
}

{ # http://rt.cpan.org/NoAuth/Bug.html?id=12676
  # setcolors() has a fencepost error
  my $img = Imager->new(xsize=>10, ysize=>10, type=>'paletted');

  is($img->addcolors(colors=>[ $black, $red ]), "0 but true",
     "add test colors");
  ok($img->setcolors(start=>1, colors=>[ $green ]), "set the last color");
  ok(!$img->setcolors(start=>2, colors=>[ $black ]), 
     "set after the last color");
}

{ # https://rt.cpan.org/Ticket/Display.html?id=20056
  # added named color support to addcolor/setcolor
  my $img = Imager->new(xsize => 10, ysize => 10, type => 'paletted');
  is($img->addcolors(colors => [ qw/000000 FF0000/ ]), "0 but true",
     "add colors as strings instead of objects");
  my @colors = $img->getcolors;
  iscolor($colors[0], $black, "check first color");
  iscolor($colors[1], $red, "check second color");
  ok($img->setcolors(colors => [ qw/00FF00 0000FF/ ]),
     "setcolors as strings instead of objects");
  @colors = $img->getcolors;
  iscolor($colors[0], $green, "check first color");
  iscolor($colors[1], $blue, "check second color");

  # make sure we handle bad colors correctly
  is($img->colorcount, 2, "start from a known state");
  is($img->addcolors(colors => [ 'XXFGXFXGXFX' ]), undef,
     "fail to add unknown color");
  is($img->errstr, 'No color named XXFGXFXGXFX found', 'check error message');
  is($img->setcolors(colors => [ 'XXFGXFXGXFXZ' ]), undef,
     "fail to set to unknown color");
  is($img->errstr, 'No color named XXFGXFXGXFXZ found', 'check error message');
}

{ # https://rt.cpan.org/Ticket/Display.html?id=20338
  # OO interface to i_glin/i_plin
  my $im = Imager->new(xsize => 10, ysize => 10, type => 'paletted');
  is($im->addcolors(colors => [ "#000", "#F00", "#0F0", "#00F" ]), "0 but true",
     "add some test colors")
    or print "# ", $im->errstr, "\n";
  # set a pixel to check
  $im->setpixel(x => 1, 'y' => 0, color => "#0F0");
  is_deeply([ $im->getscanline('y' => 0, type=>'index') ],
            [ 0, 2, (0) x 8 ], "getscanline index in list context");
  isbin($im->getscanline('y' => 0, type=>'index'),
        "\x00\x02" . "\x00" x 8,
        "getscanline index in scalar context");
  is($im->setscanline('y' => 0, pixels => [ 1, 2, 0, 3 ], type => 'index'),
     4, "setscanline with list");
  is($im->setscanline('y' => 0, x => 4, pixels => pack("C*", 3, 2, 1, 0, 3),
                      type => 'index'),
     5, "setscanline with pv");
  is_deeply([ $im->getscanline(type => 'index', 'y' => 0) ],
            [ 1, 2, 0, 3, 3, 2, 1, 0, 3, 0 ],
            "check values set");
  eval { # should croak on OOR index
    $im->setscanline('y' => 1, pixels => [ 255 ], type=>'index');
  };
  ok($@, "croak on setscanline() to invalid index");
  eval { # same again with pv
    $im->setscanline('y' => 1, pixels => "\xFF", type => 'index');
  };
  ok($@, "croak on setscanline() with pv to invalid index");
}

sub iscolor {
  my ($c1, $c2, $msg) = @_;

  my $builder = Test::Builder->new;
  my @c1 = $c1->rgba;
  my @c2 = $c2->rgba;
  if (!$builder->ok($c1[0] == $c2[0] && $c1[1] == $c2[1] && $c1[2] == $c2[2],
                    $msg)) {
    $builder->diag(<<DIAG);
      got color: [ @c1 ]
 expected color: [ @c2 ]
DIAG
  }
}

sub isbin ($$$) {
  my ($got, $expected, $msg) = @_;

  my $builder = Test::Builder->new;
  if (!$builder->ok($got eq $expected, $msg)) {
    (my $got_dec = $got) =~ s/([^ -~])/sprintf("\\x%02x", ord $1)/ge;
    (my $exp_dec = $expected)  =~ s/([^ -~])/sprintf("\\x%02x", ord $1)/ge;
    $builder->diag(<<DIAG);
      got: "$got_dec"
 expected: "$exp_dec"
DIAG
  }
}

sub coloreq {
  my ($left, $right, $comment) = @_;

  my ($rl, $gl, $bl, $al) = $left->rgba;
  my ($rr, $gr, $br, $ar) = $right->rgba;

  print "# comparing color($rl,$gl,$bl,$al) with ($rr,$gr,$br,$ar)\n";
  ok($rl == $rr && $gl == $gr && $bl == $br && $al == $ar,
      $comment);
}

