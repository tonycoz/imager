#!perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

use Test::More;

use Imager;
use Imager::Test qw(is_fcolor3 is_fcolor4 is_color4);

-d "testout" or mkdir "testout";

Imager->open_log(log => "testout/t15color.log");

my $c1 = Imager::Color->new(100, 150, 200, 250);
is_color4($c1, 100, 150, 200, 250, 'simple 4-arg');
my $c2 = Imager::Color->new(100, 150, 200);
is_color4($c2, 100, 150, 200, 255, 'simple 3-arg');
my $c3 = Imager::Color->new("#6496C8");
is_color4($c3, 100, 150, 200, 255, 'web color');
# crashes in Imager-0.38pre8 and earlier
my @foo;
for (1..1000) {
  push(@foo, Imager::Color->new("#FFFFFF"));
}
my $fail;
for (@foo) {
  Imager::Color::set_internal($_, 128, 128, 128, 128) == $_ or ++$fail;
  Imager::Color::set_internal($_, 128, 129, 130, 131) == $_ or ++$fail;
  test_col($_, 128, 129, 130, 131) or ++$fail;
}
ok(!$fail, 'consitency check');

# test the new OO methods
SKIP:
{
  skip "no X rgb.txt found", 1 
    unless grep -r, Imager::Color::_test_x_palettes();
  is_color4(Imager::Color->new(xname=>'blue'), 0, 0, 255, 255, 'xname');
}

my @oo_tests =
  (
   [
    [ r=>100, g=>150, b=>200 ],
    100, 150, 200, 255,
    'r g b'
   ],
   [
    [ red=>101, green=>151, blue=>201 ],
    101, 151, 201, 255,
    'red green blue'
   ],
   [
    [ grey=>102 ],
    102, 255, 255, 255,
    'grey'
   ],
   [
    [ gray=>103 ],
    103, 255, 255, 255,
    'gray'
   ],
   [
    [ gimp=>'snow' , palette=>'testimg/test_gimp_pal' ],
    255, 250, 250, 255,
    'gimp'
   ],
   [
    [ h=>0, 's'=>0, 'v'=>1.0 ],
    255, 255, 255, 255,
    'h s v'
   ],
   [
    [ h=>0, 's'=>1, v=>1 ],
    255, 0, 0, 255,
    'h s v again'
   ],
   [
    [ web=>'#808182' ],
    128, 129, 130, 255,
    'web 6 digit'
   ],
   [
    [ web=>'#123' ],
    0x11, 0x22, 0x33, 255,
    'web 3 digit'
   ],
   [
    [ rgb=>[ 255, 150, 121 ] ],
    255, 150, 121, 255,
    'rgb arrayref'
   ],
   [
    [ rgba=>[ 255, 150, 121, 128 ] ],
    255, 150, 121, 128,
    'rgba arrayref'
   ],
   [
    [ hsv=>[ 0, 1, 1 ] ],
    255, 0, 0, 255,
    'hsv arrayref'
   ],
   [
    [ channel0=>129, channel1=>130, channel2=>131, channel3=>134 ],
    129, 130, 131, 134,
    'channel0-3'
   ],
   [
    [ c0=>129, c1=>130, c2=>131, c3=>134 ],
    129, 130, 131, 134,
    'c0-3',
   ],
   [
    [ channels=>[ 200, ] ],
    200, 0, 0, 0,
    'channels arrayref (1)'
   ],
   [
    [ channels=>[ 200, 201 ] ],
    200, 201, 0, 0,
    'channels arrayref (2)'
   ],
   [
    [ channels=>[ 200, 201, 203 ] ],
    200, 201, 203, 0,
    'channels arrayref (3)'
   ],
   [
    [ channels=>[ 200, 201, 203, 204 ] ],
    200, 201, 203, 204,
    'channels arrayref (4)'
   ],
   [
    [ name=>'snow', palette=>'testimg/test_gimp_pal' ],
    255, 250, 250, 255,
    'name'
   ],

   # rgb() without alpha
   [
     [ "rgb(255 128 128)" ],
     255, 128, 128, 255,
     "rgb non-percent, spaces"
    ],
   [
     [ "rgb(255, 128, 128)" ],
     255, 128, 128, 255,
     "rgb non-percent, commas simple"
    ],
   [
     [ "rgb(255 ,128   ,128)" ],
     255, 128, 128, 255,
     "rgb non-percent, commas less simple"
    ],
   [
     [ "rgb(254.5 127.5 127.5)" ],
     255, 128, 128, 255,
     "rgb non-percent with decimals, spaces"
    ],
   [
     [ "rgb(254.5,127.5,126.2)" ],
     255, 128, 127, 255,
     "rgb non-percent decimals, commas"
    ],
   [
     [ "rgb(254.5  , 127.5 ,  126.2)" ],
     255, 128, 127, 255,
     "rgb non-percent decimals, commas more spaces"
    ],
   [
     [ "rgb(100% 50% 50%)" ],
     255, 128, 128, 255,
     "rgb percent, spaces"
    ],
   [
     [ "rgb(100%, 50%, 50%)" ],
     255, 128, 128, 255,
     "rgb percent, commas"
    ],
   [
     [ "rgb(99.99% 49.99% 74.98%)" ],
     255, 128, 192, 255,
     "rgb percent decimals, spaces"
    ],
   [
     [ "rgb(99.99%, 49.99%, 49.98%)" ],
     255, 128, 128, 255,
     "rgb percent decimals, commas"
    ],
   # rgb() with alpha
   [
     [ "rgb(255 128 128 / 0.5)" ],
     255, 128, 128, 128,
     "rgba non-percent, spaces"
    ],
   [
     [ "rgb(255, 128, 128, 0.25)" ],
     255, 128, 128, 64,
     "rgba non-percent, commas simple"
    ],
   [
     [ "rgb(255 ,128   ,128 , 0.75)" ],
     255, 128, 128, 192,
     "rgba non-percent, commas less simple"
    ],
   [
     [ "rgba(254.5 127.5 127.5 / 0.1)" ],
     255, 128, 128, 26,
     "rgba non-percent with decimals, spaces"
    ],
   [
     [ "rgb(254.5,127.5,126.2,1.0)" ],
     255, 128, 127, 255,
     "rgba non-percent decimals, commas"
    ],
   [
     [ "rgb(254.5  , 127.5 ,  126.2, 0.9)" ],
     255, 128, 127, 230,
     "rgba non-percent decimals, commas more spaces"
    ],
   [
     [ "rgb(100% 50% 50% / 0.2)" ],
     255, 128, 128, 51,
     "rgba percent, spaces"
    ],
   [
     [ "rgb(100%, 50%, 50%, 30%)" ],
     255, 128, 128, 77,
     "rgba percent, commas"
    ],
   [
     [ "rgb(99.99% 49.99% 74.98% / 49.9%)" ],
     255, 128, 192, 128,
     "rgba percent decimals, spaces"
    ],
   [
     [ "rgb(99.99%, 49.99%, 49.98%, 50.0%)" ],
     255, 128, 128, 128,
     "rgba percent decimals, commas"
    ],
  );

for my $test (@oo_tests) {
  my ($parms, $r, $g, $b, $a, $name) = @$test;
  is_color4(Imager::Color->new(@$parms), $r, $g, $b, $a, $name);
}

# test the internal HSV <=> RGB conversions
# these values were generated using the GIMP
# all but hue is 0..360, saturation and value from 0 to 1
# rgb from 0 to 255
my @hsv_vs_rgb =
  (
   { hsv => [ 0, 0.2, 0.1 ], rgb=> [ 25, 20, 20 ] },
   { hsv => [ 0, 0.5, 1.0 ], rgb => [ 255, 127, 127 ] },
   { hsv => [ 100, 0.5, 1.0 ], rgb => [ 170, 255, 127 ] },
   { hsv => [ 100, 1.0, 1.0 ], rgb=> [ 85, 255, 0 ] },
   { hsv => [ 335, 0.5, 0.5 ], rgb=> [127, 63, 90 ] },
  );

use Imager::Color::Float;
my $test_num = 23;
my $index = 0;
for my $entry (@hsv_vs_rgb) {
  print "# color index $index\n";
  my $hsv = $entry->{hsv};
  my $rgb = $entry->{rgb};
  my $fhsvo = Imager::Color::Float->new($hsv->[0]/360.0, $hsv->[1], $hsv->[2]);
  my $fc = Imager::Color::Float::i_hsv_to_rgb($fhsvo);
  is_fcolor3($fc, $rgb->[0]/255, $rgb->[1]/255, $rgb->[2]/255, 0.01,
	     "i_hsv_to_rgbf $index");
  my $fc2 = Imager::Color::Float::i_rgb_to_hsv($fc);
  is_fcolor3($fc2, $hsv->[0]/360.0, $hsv->[1], $hsv->[2], "i_rgbf_to_hsv $index");

  my $hsvo = Imager::Color->new($hsv->[0]*255/360.0, $hsv->[1] * 255, 
                                $hsv->[2] * 255);
  my $c = Imager::Color::i_hsv_to_rgb($hsvo);
  color_close_enough("i_hsv_to_rgb $index", @$rgb, $c);
  my $c2 = Imager::Color::i_rgb_to_hsv($c);
  color_close_enough_hsv("i_rgb_to_hsv $index", $hsv->[0]*255/360.0, $hsv->[1] * 255, 
                     $hsv->[2] * 255, $c2);
  ++$index;
}

# check the built-ins table
is_color4(Imager::Color->new(builtin=>'black'), 0, 0, 0, 255, 'builtin black');

{
  my $c1 = Imager::Color->new(255, 255, 255, 0);
  my $c2 = Imager::Color->new(255, 255, 255, 255);
  ok(!$c1->equals(other=>$c2), "not equal no ignore alpha");
  ok(scalar($c1->equals(other=>$c2, ignore_alpha=>1)), 
      "equal with ignore alpha");
  ok($c1->equals(other=>$c1), "equal to itself");
}

{ # http://rt.cpan.org/NoAuth/Bug.html?id=13143
  # Imager::Color->new(color_name) warning if HOME environment variable not set
  local $ENV{HOME};
  my @warnings;
  local $SIG{__WARN__} = sub { push @warnings, "@_" };

  # presumably no-one will name a color like this.
  my $c1 = Imager::Color->new(gimp=>"ABCDEFGHIJKLMNOP");
  is(@warnings, 0, "Should be no warnings")
    or do { print "# $_" for @warnings };
}

{
  # float color from hex triple
  my $f3white = Imager::Color::Float->new("#FFFFFF");
  is_fcolor4($f3white, 1.0, 1.0, 1.0, 1.0, "check color #FFFFFF");
  my $f3black = Imager::Color::Float->new("#000000");
  is_fcolor4($f3black, 0, 0, 0, 1.0, "check color #000000");
  my $f3grey = Imager::Color::Float->new("#808080");
  is_fcolor4($f3grey, 0x80/0xff, 0x80/0xff, 0x80/0xff, 1.0, "check color #808080");

  my $f4white = Imager::Color::Float->new("#FFFFFF80");
  is_fcolor4($f4white, 1.0, 1.0, 1.0, 0x80/0xff, "check color #FFFFFF80");
}

{
  # fail to make a color
  ok(!Imager::Color::Float->new("-unknown-"), "try to make float color -unknown-");
}

{
  # set after creation
  my $c = Imager::Color::Float->new(0, 0, 0);
  is_fcolor4($c, 0, 0, 0, 1.0, "check simple init of float color");
  ok($c->set(1.0, 0.5, 0.25, 1.0), "set() the color");
  is_fcolor4($c, 1.0, 0.5, 0.25, 1.0, "check after set");

  ok(!$c->set("-unknown-"), "set to unknown");
}

{
  # test ->hsv
  my $c = Imager::Color->new(255, 0, 0);
  my($h,$s,$v) = $c->hsv;
  is($h,0,'red hue');
  is($s,1,'red saturation');
  is($v,1,'red value');

  $c = Imager::Color->new(0, 255, 0);
  ($h,$s,$v) = $c->hsv;
  is($h,120,'green hue');
  is($s,1,'green saturation');
  is($v,1,'green value');

  $c = Imager::Color->new(0, 0, 255);
  ($h,$s,$v) = $c->hsv;
  is($h,240,'blue hue');
  is($s,1,'blue saturation');
  is($v,1,'blue value');

  $c = Imager::Color->new(255, 255, 255);
  ($h,$s,$v) = $c->hsv;
  is($h,0,'white hue');
  is($s,0,'white saturation');
  is($v,1,'white value');

  $c = Imager::Color->new(0, 0, 0);
  ($h,$s,$v) = $c->hsv;
  is($h,0,'black hue');
  is($s,0,'black saturation');
  is($v,0,'black value');
}

# test conversion to float color
{
  my $c = Imager::Color->new(255, 128, 64);
  my $cf = $c->as_float;
  is_fcolor4($cf, 1.0, 128/255, 64/255, 1.0, "check color converted to float");
}

# test conversion to 8bit color
{
  # more cases than above
  my @tests =
    (
      [ "simple black", [ 0, 0, 0 ], [ 0, 0, 0, 255 ] ],
      [ "range clip", [ -0.01, 0, 1.01 ], [ 0, 0, 255, 255 ] ],
     );
  for my $test (@tests) {
    my ($name, $float, $expect) = @$test;
    my $f = Imager::Color::Float->new(@$float);
    {
      # test as_8bit
      my $as8 = $f->as_8bit;
      is_deeply([ $as8->rgba ], $expect, $name);
    }
    {
      # test construction
      my $as8 = Imager::Color->new($f);
      is_deeply([ $as8->rgba ], $expect, "constructed: $name");
    }
  }
}

# test conversion from 8bit to float color
{
  my @tests =
    (
      [ "black", [ 0,0,0 ], [ 0,0,0, 1.0 ] ],
      [ "white", [ 255, 255, 255 ], [ 1.0, 1.0, 1.0, 1.0 ] ],
      [ "dark red", [ 128, 0, 0 ], [ 128/255, 0, 0, 1.0 ] ],
      [ "green", [ 255, 128, 128, 64 ], [ 1.0, 128/255, 128/255, 64/255 ] ],
     );
  for my $test (@tests) {
    my ($name, $as8, $float) = @$test;
    my $c = Imager::Color->new(@$as8);
    {
      # test as_float
      my $f = $c->as_float;
      is_fcolor4($f, $float->[0], $float->[1], $float->[2], $float->[3], "as_float: $name");
    }
    {
      # test construction
      my $f = Imager::Color::Float->new($c);
      is_fcolor4($f, $float->[0], $float->[1], $float->[2], $float->[3], "construction: $name");
    }
  }
}

{
  # CSS rgb() support for float colors
  my @tests =
    (
      # rgb() without alpha
      [
        [ "rgb(255 128 128)" ],
        1.0, 128/255, 128/255, 1.0,
        "rgb non-percent, spaces"
       ],
      [
        [ "rgb(255, 128, 128)" ],
        1.0, 128/255, 128/255, 1.0,
        "rgb non-percent, commas simple"
       ],
      [
        [ "rgb(255 ,128   ,128)" ],
        1.0, 128/255, 128/255, 1.0,
        "rgb non-percent, commas less simple"
       ],
      [
        [ "rgb(254.5 127.5 127.5)" ],
        254.5/255, 127.5/255, 127.5/255, 1.0,
        "rgb non-percent with decimals, spaces"
       ],
      [
        [ "rgb(254.5,127.5,126.2)" ],
        254.5/255, 127.5/255, 126.2/255, 1.0,
        "rgb non-percent decimals, commas"
       ],
      [
        [ "rgb(254.5  , 127.5 ,  126.2)" ],
        254.5/255, 127.5/255, 126.2/255, 1.0,
        "rgb non-percent decimals, commas more spaces"
       ],
      [
        [ "rgb(100% 50% 50%)" ],
        1.0, 0.5, 0.5, 1.0,
        "rgb percent, spaces"
       ],
      [
        [ "rgb(100%, 50%, 50%)" ],
        1.0, 0.5, 0.5, 1.0,
        "rgb percent, commas"
       ],
      [
        [ "rgb(99.99% 49.99% 74.98%)" ],
        0.9999, 0.4999, 0.7498, 1.0,
        "rgb percent decimals, spaces"
       ],
      [
        [ "rgb(99.99%, 49.99%, 49.98%)" ],
        0.9999, 0.4999, 0.4998, 1.0,
        "rgb percent decimals, commas"
       ],
      # rgb() with alpha
      [
        [ "rgb(255 128 128 / 0.5)" ],
        1.0, 128/255, 128/255, 0.5,
        "rgba non-percent, spaces"
       ],
      [
        [ "rgb(255, 128, 128, 0.25)" ],
        1.0, 128/255, 128/255, 0.25,
        "rgba non-percent, commas simple"
       ],
      [
        [ "rgb(255 ,128   ,128 , 0.75)" ],
        1.0, 128/255, 128/255, 0.75,
        "rgba non-percent, commas less simple"
       ],
      [
        [ "rgba(254.5 127.5 127.5 / 0.1)" ],
        254.5/255, 127.5/255, 127.5/255, 0.1,
        "rgba non-percent with decimals, spaces"
       ],
      [
        [ "rgb(254.5,127.5,126.2,1.0)" ],
        254.5/255, 127.5/255, 126.2/255, 1.0,
        "rgba non-percent decimals, commas"
       ],
      [
        [ "rgb(254.5  , 127.5 ,  126.2, 0.9)" ],
        254.5/255, 127.5/255, 126.2/255, 0.9,
        "rgba non-percent decimals, commas more spaces"
       ],
      [
        [ "rgb(100% 50% 50% / 0.2)" ],
        1.0, 0.5, 0.5, 0.2,
        "rgba percent, spaces"
       ],
      [
        [ "rgb(100%, 50%, 50%, 30%)" ],
        1.0, 0.5, 0.5, 0.3,
        "rgba percent, commas"
       ],
      [
        [ "rgb(99.99% 49.99% 74.98% / 49.9%)" ],
        0.9999, 0.4999, 0.7498, 0.499,
        "rgba percent decimals, spaces"
       ],
      [
        [ "rgb(99.99%, 49.99%, 49.98%, 50.0%)" ],
        0.9999, 0.4999, 0.4998, 0.5,
        "rgba percent decimals, commas"
       ],
     );
  for my $test (@tests) {
    my ($parms, $r, $g, $b, $a, $name) = @$test;
    is_fcolor4(Imager::Color::Float->new(@$parms), $r, $g, $b, $a, "float: $name");
  }
}

done_testing();

sub test_col {
  my ($c, $r, $g, $b, $a) = @_;
  unless ($c) {
    print "# $Imager::ERRSTR\n";
    return 0;
  }
  my ($cr, $cg, $cb, $ca) = $c->rgba;
  return $r == $cr && $g == $cg && $b == $cb && $a == $ca;
}

sub color_close_enough {
  my ($name, $r, $g, $b, $c) = @_;

  my ($cr, $cg, $cb) = $c->rgba;
  ok(abs($cr-$r) <= 5 && abs($cg-$g) <= 5 && abs($cb-$b) <= 5,
    "$name - ($cr, $cg, $cb) <=> ($r, $g, $b)");
}

sub color_close_enough_hsv {
  my ($name, $h, $s, $v, $c) = @_;

  my ($ch, $cs, $cv) = $c->rgba;
  if ($ch < 5 && $h > 250) {
    $ch += 255;
  }
  elsif ($ch > 250 && $h < 5) {
    $h += 255;
  }
  ok(abs($ch-$h) <= 5 && abs($cs-$s) <= 5 && abs($cv-$v) <= 5,
    "$name - ($ch, $cs, $cv) <=> ($h, $s, $v)");
}

