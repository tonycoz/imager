#!/usr/bin/perl -w
use strict;
use Imager;
use Test::More tests => 9;

# extracted from t/t36oofont.t

my $fontname_pfb = "fontfiles/dcr10.pfb";

my $green=Imager::Color->new(92,205,92,128);
die $Imager::ERRSTR unless $green;
my $red=Imager::Color->new(205, 92, 92, 255);
die $Imager::ERRSTR unless $red;

ok((-d "testout" or mkdir "testout"), "make output directory");

init_log("testout/t20oo.log", 1);

my $img=Imager->new(xsize=>300, ysize=>100) or die "$Imager::ERRSTR\n";

my $font=Imager::Font->new(file=>$fontname_pfb,size=>25, type => "t1")
  or die $img->{ERRSTR};

ok(1, "created font");

ok($img->string(font=>$font, text=>"XMCLH", 'x'=>100, 'y'=>100),
   "draw text");
$img->line(x1=>0, x2=>300, y1=>50, y2=>50, color=>$green);

my $text="LLySja";
my @bbox=$font->bounding_box(string=>$text, 'x'=>0, 'y'=>50);

is(@bbox, 8, "bounding box list length");

$img->box(box=>\@bbox, color=>$green);

# "utf8" support
$text = pack("C*", 0x41, 0xE2, 0x80, 0x90, 0x41);
ok($img->string(font=>$font, text=>$text, 'x'=>100, 'y'=>50, utf8=>1,
		overline=>1),
   "draw 'utf8' hand-encoded text");

ok($img->string(font=>$font, text=>$text, 'x'=>140, 'y'=>50, utf8=>1, 
		underline=>1, channel=>2),
   "channel 'utf8' hand-encoded text");

SKIP:
{
  $] >= 5.006
    or skip("perl too old for native utf8", 2);
    eval q{$text = "A\x{2010}A"};
  ok($img->string(font=>$font, text=>$text, 'x'=>180, 'y'=>50,
		  strikethrough=>1),
     "draw native UTF8 text");
  ok($img->string(font=>$font, text=>$text, 'x'=>220, 'y'=>50, channel=>1),
     "channel native UTF8 text");
}

ok($img->write(file=>"testout/t36oofont1.ppm", type=>'pnm'),
   "write t36oofont1.ppm")
  or print "# ",$img->errstr,"\n";

unless ($ENV{IMAGER_KEEP_FILES}) {
  unlink "testout/t36oofont1.ppm";
}
