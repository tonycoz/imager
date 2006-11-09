#!perl -w
use strict;
use Test::More tests => 11;

BEGIN { use_ok('Imager') }

require_ok('Imager::Font::Wrap');

my $img = Imager->new(xsize=>400, ysize=>400);

my $text = <<EOS;
This is a test of text wrapping. This is a test of text wrapping. This =
is a test of text wrapping. This is a test of text wrapping. This is a =
test of text wrapping. This is a test of text wrapping. This is a test =
of text wrapping. This is a test of text wrapping. This is a test of =
text wrapping. XX.

Xxxxxxxxxxxxxxxxxxxxxxxxxxxwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww xxxx.

This is a test of text wrapping. This is a test of text wrapping. This =
is a test of text wrapping. This is a test of text wrapping. This is a =
test of text wrapping. This is a test of text wrapping. This is a test =
of text wrapping. This is a test of text wrapping. This is a test of =
text wrapping. This is a test of text wrapping. This is a test of text =
wrapping. This is a test of text wrapping. This is a test of text =
wrapping. This is a test of text wrapping. This is a test of text =
wrapping. This is a test of text wrapping. This is a test of text =
wrapping. XX.
EOS

$text =~ s/=\n//g;

my $fontfile = $ENV{WRAPTESTFONT} || $ENV{TTFONTTEST} || "fontfiles/ImUgly.ttf";

my $font = Imager::Font->new(file=>$fontfile);

SKIP:
{
  Imager::i_has_format('tt') || Imager::i_has_format('ft2')
      or skip("Need Freetype 1.x or 2.x to test", 9);

  ok($font, "loading font")
    or skip("Could not load test font", 8);

  Imager::Font->priorities(qw(t1 ft2 tt));
  ok(scalar Imager::Font::Wrap->wrap_text(string => $text,
                                font=>$font,
                                image=>$img,
                                size=>13,
                                width => 380, aa=>1,
                                x=>10, 'y'=>10,
                                justify=>'fill',
                                color=>'FFFFFF'),
      "basic test");
  ok($img->write(file=>'testout/t80wrapped.ppm'), "save to file");
  ok(scalar Imager::Font::Wrap->wrap_text(string => $text,
                                font=>$font,
                                image=>undef,
                                size=>13,
                                width => 380,
                                x=>10, 'y'=>10,
                                justify=>'left',
                                color=>'FFFFFF'),
      "no image test");
  my $bbox = $font->bounding_box(string=>"Xx", size=>13);
  ok($bbox, "get height for check");

  my $used;
  ok(scalar Imager::Font::Wrap->wrap_text
      (string=>$text, font=>$font, image=>undef, size=>13, width=>380,
       savepos=> \$used, height => $bbox->font_height), "savepos call");
  ok($used > 20 && $used < length($text), "savepos value");
  print "# $used\n";
  my @box = Imager::Font::Wrap->wrap_text
    (string=>substr($text, 0, $used), font=>$font, image=>undef, size=>13,
     width=>380);

  ok(@box == 4, "bounds list count");
  print "# @box\n";
  ok($box[3] == $bbox->font_height, "check height");
}
