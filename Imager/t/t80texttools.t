use strict;
my $loaded;
BEGIN { 
  require "t/testtools.pl";
  $| = 1; print "1..11\n"; 
}
END { okx(0, "loading") unless $loaded; }
use Imager;
$loaded = 1;

okx(1, "Loaded");

requireokx("Imager/Font/Wrap.pm", "load basic wrapping");

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

unless (Imager::i_has_format('tt') || Imager::i_has_format('ft2')) {
  skipx(9, "Need Freetype 1.x or 2.x to test");
  exit;
}

if (okx($font, "loading font")) {
  Imager::Font->priorities(qw(t1 ft2 tt));
  okx(scalar Imager::Font::Wrap->wrap_text(string => $text,
                                font=>$font,
                                image=>$img,
                                size=>13,
                                width => 380, aa=>1,
                                x=>10, 'y'=>10,
                                justify=>'fill',
                                color=>'FFFFFF'),
      "basic test");
  okx($img->write(file=>'testout/t80wrapped.ppm'), "save to file");
  okx(scalar Imager::Font::Wrap->wrap_text(string => $text,
                                font=>$font,
                                image=>undef,
                                size=>13,
                                width => 380,
                                x=>10, 'y'=>10,
                                justify=>'left',
                                color=>'FFFFFF'),
      "no image test");
  my $bbox = $font->bounding_box(string=>"Xx", size=>13);
  okx($bbox, "get height for check");

  my $used;
  okx(scalar Imager::Font::Wrap->wrap_text
      (string=>$text, font=>$font, image=>undef, size=>13, width=>380,
       savepos=> \$used, height => $bbox->font_height), "savepos call");
  okx($used > 20 && $used < length($text), "savepos value");
  print "# $used\n";
  my @box = Imager::Font::Wrap->wrap_text
    (string=>substr($text, 0, $used), font=>$font, image=>undef, size=>13,
     width=>380);

  okx(@box == 4, "bounds list count");
  print "# @box\n";
  okx($box[3] == $bbox->font_height, "check height");
}
else {
  skipx(8, "Could not load test font");
}
