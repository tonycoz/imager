#!/usr/bin/perl -w
use strict;

#use lib qw(blib/lib blib/arch);

# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

my $loaded;
BEGIN { $| = 1; print "1..13\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;
require "t/testtools.pl";
$loaded=1;
okx(1, "loaded");

init_log("testout/t36oofont.log", 1);

my $fontname_tt=$ENV{'TTFONTTEST'}||'./fontfiles/dodge.ttf';
my $fontname_pfb=$ENV{'T1FONTTESTPFB'}||'./fontfiles/dcr10.pfb';


my $green=Imager::Color->new(92,205,92,128);
die $Imager::ERRSTR unless $green;
my $red=Imager::Color->new(205, 92, 92, 255);
die $Imager::ERRSTR unless $red;

if (i_has_format("t1") and -f $fontname_pfb) {

  my $img=Imager->new(xsize=>300, ysize=>100) or die "$Imager::ERRSTR\n";

  my $font=Imager::Font->new(file=>$fontname_pfb,size=>25)
    or die $img->{ERRSTR};

  okx(1, "created font");

  okx($img->string(font=>$font, text=>"XMCLH", 'x'=>100, 'y'=>100),
      "draw text");
  $img->line(x1=>0, x2=>300, y1=>50, y2=>50, color=>$green);

  my $text="LLySja";
  my @bbox=$font->bounding_box(string=>$text, 'x'=>0, 'y'=>50);

  okx(@bbox == 6, "bounding box list length");

  $img->box(box=>\@bbox, color=>$green);

  okx($img->write(file=>"testout/t36oofont1.ppm", type=>'pnm'),
      "write t36oofont1.ppm")
    or print "# ",$img->errstr,"\n";

} else {
  skipx(4, "T1lib missing or disabled");
}

if (i_has_format("tt") and -f $fontname_tt) {

  my $img=Imager->new(xsize=>300, ysize=>100) or die "$Imager::ERRSTR\n";

  my $font=Imager::Font->new(file=>$fontname_tt,size=>25)
    or die $img->{ERRSTR};

  okx(1, "create TT font object");

  okx($img->string(font=>$font, text=>"XMCLH", 'x'=>100, 'y'=>100),
      "draw text");

  $img->line(x1=>0, x2=>300, y1=>50, y2=>50, color=>$green);

  my $text="LLySja";
  my @bbox=$font->bounding_box(string=>$text, 'x'=>0, 'y'=>50);

  okx(@bbox == 6, "bbox list size");

  $img->box(box=>\@bbox, color=>$green);

  $text = pack("C*", 0x41, 0xE2, 0x80, 0x90, 0x41);
  okx($img->string(font=>$font, text=>$text, 'x'=>100, 'y'=>50, utf8=>1),
      "draw hand-encoded UTF8 text");

  if($] >= 5.006) {
    eval q{$text = "A\x{2010}A"};
    okx($img->string(font=>$font, text=>$text, 'x'=>200, 'y'=>50),
       "draw native UTF8 text");
  }
  else {
    skipx(1, "perl too old for native utf8");
  }

  okx($img->write(file=>"testout/t36oofont2.ppm", type=>'pnm'),
      "write t36oofont2.ppm")
    or print "# ", $img->errstr,"\n";

  okx($font->utf8, "make sure utf8 method returns true");

} else {
  skipx(7, "FT1.x missing or disabled");
}

okx(1, "end");
