#!/usr/bin/perl -w
use strict;

#use lib qw(blib/lib blib/arch);

# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..7\n"; }
END {print "not ok 1\n" unless $::loaded;}
use Imager;
$::loaded=1;
print "ok 1\n";

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

  print "ok 2\n";

  $img->string(font=>$font, text=>"XMCLH", 'x'=>100, 'y'=>100) 
    or die $img->{ERRSTR};

  print "ok 3\n";

  $img->line(x1=>0, x2=>300, y1=>50, y2=>50, color=>$green);

  my $text="LLySja";
  my @bbox=$font->bounding_box(string=>$text, 'x'=>0, 'y'=>50);

  print @bbox ? '' : 'not ', "ok 4\n";

  $img->box(box=>\@bbox, color=>$green);

  $img->write(file=>"testout/t36oofont1.ppm", type=>'pnm')
    or die "cannot write to testout/t36oofont1.ppm: $img->{ERRSTR}\n";

} else {
  print "ok 2 # skip\n";
  print "ok 3 # skip\n";
  print "ok 4 # skip\n";
}

if (i_has_format("tt") and -f $fontname_tt) {

  my $img=Imager->new(xsize=>300, ysize=>100) or die "$Imager::ERRSTR\n";

  my $font=Imager::Font->new(file=>$fontname_tt,size=>25)
    or die $img->{ERRSTR};

  print "ok 5\n";

  $img->string(font=>$font, text=>"XMCLH", 'x'=>100, 'y'=>100) 
    or die $img->{ERRSTR};

  print "ok 6\n";

  $img->line(x1=>0, x2=>300, y1=>50, y2=>50, color=>$green);

  my $text="LLySja";
  my @bbox=$font->bounding_box(string=>$text, 'x'=>0, 'y'=>50);

  print @bbox ? '' : 'not ', "ok 7\n";

  $img->box(box=>\@bbox, color=>$green);

  $img->write(file=>"testout/t36oofont2.ppm", type=>'pnm')
    or die "cannot write to testout/t36oofont2.ppm: $img->{ERRSTR}\n";

} else {
  print "ok 5 # skip\n";
  print "ok 6 # skip\n";
  print "ok 7 # skip\n";
}

