#!/usr/bin/perl -w
use strict;

#use lib qw(blib/lib blib/arch);

# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)
use Test::More tests => 16;

BEGIN { use_ok('Imager') };

BEGIN {
  require Imager::Test;
  Imager::Test->import(qw(isnt_image));
}

-d "testout" or mkdir "testout";

Imager->open_log(log => "testout/t36oofont.log");

my $fontname_tt=$ENV{'TTFONTTEST'}||'./fontfiles/dodge.ttf';

my $green=Imager::Color->new(92,205,92,128);
die $Imager::ERRSTR unless $green;
my $red=Imager::Color->new(205, 92, 92, 255);
die $Imager::ERRSTR unless $red;

SKIP:
{
  $Imager::formats{"tt"} && -f $fontname_tt
    or skip("FT1.x missing or disabled", 14);

  my $img=Imager->new(xsize=>300, ysize=>100) or die "$Imager::ERRSTR\n";

  my $font=Imager::Font->new(file=>$fontname_tt,size=>25)
    or die $img->{ERRSTR};

  ok(1, "create TT font object");

  ok($img->string(font=>$font, text=>"XMCLH", 'x'=>100, 'y'=>100),
      "draw text");

  $img->line(x1=>0, x2=>300, y1=>50, y2=>50, color=>$green);

  my $text="LLySja";
  my @bbox=$font->bounding_box(string=>$text, 'x'=>0, 'y'=>50);

  is(@bbox, 8, "bbox list size");

  $img->box(box=>\@bbox, color=>$green);

  $text = pack("C*", 0x41, 0xE2, 0x80, 0x90, 0x41);
  ok($img->string(font=>$font, text=>$text, 'x'=>100, 'y'=>50, utf8=>1),
      "draw hand-encoded UTF8 text");

 SKIP:
  {
    $] >= 5.006
      or skip("perl too old for native utf8", 1);
    eval q{$text = "A\x{2010}A"};
    ok($img->string(font=>$font, text=>$text, 'x'=>200, 'y'=>50),
       "draw native UTF8 text");
  }

  ok($img->write(file=>"testout/t36oofont2.ppm", type=>'pnm'),
      "write t36oofont2.ppm")
    or print "# ", $img->errstr,"\n";

  ok($font->utf8, "make sure utf8 method returns true");

  my $has_chars = $font->has_chars(string=>"\x01A");
  is($has_chars, "\x00\x01", "has_chars scalar");
  my @has_chars = $font->has_chars(string=>"\x01A");
  ok(!$has_chars[0], "has_chars list 0");
  ok($has_chars[1], "has_chars list 1");

  { # RT 71469
    my $font1 = Imager::Font->new(file => $fontname_tt, type => "tt");
    my $font2 = Imager::Font::Truetype->new(file => $fontname_tt);

    for my $font ($font1, $font2) {
      print "# ", join(",", $font->{color}->rgba), "\n";

      my $im = Imager->new(xsize => 20, ysize => 20, channels => 4);

      ok($im->string(text => "T", font => $font, y => 15),
	 "draw with default color")
	or print "# ", $im->errstr, "\n";
      my $work = Imager->new(xsize => 20, ysize => 20);
      my $cmp = $work->copy;
      $work->rubthrough(src => $im);
      isnt_image($work, $cmp, "make sure something was drawn");
    }
  }
}

ok(1, "end");
