#!perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..14\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager qw(:all);
$loaded = 1;
print "ok 1\n";

init_log("testout/t38ft2font.log",2);

sub skip { 
  for (2..14) {
    print "ok $_ # skip no Freetype2 library\n";
  }
  malloc_state();
  exit(0);
}

if (!(i_has_format("ft2")) ) { skip(); }
print "# has ft2\n";

$fontname=$ENV{'TTFONTTEST'}||'./fontfiles/dodge.ttf';

if (! -f $fontname) {
  print "# cannot find fontfile for truetype test $fontname\n";
  skip();	
}

i_init_fonts();
#     i_tt_set_aa(1);

$bgcolor=i_color_new(255,0,0,0);
$overlay=Imager::ImgRaw::new(200,70,3);

$ttraw=Imager::Font::FreeType2::i_ft2_new($fontname, 0);

$ttraw or print Imager::_error_as_msg(),"\n";
#use Data::Dumper;
#warn Dumper($ttraw);

@bbox=Imager::Font::FreeType2::i_ft2_bbox($ttraw, 50.0, 0, 'XMCLH', 0);
print "#bbox: ($bbox[0], $bbox[1]) - ($bbox[2], $bbox[3])\n";

Imager::Font::FreeType2::i_ft2_cp($ttraw,$overlay,5,50,1,50.0,50, 'XMCLH',1,1, 0, 0);
i_draw($overlay,0,50,100,50,$bgcolor);

open(FH,">testout/t38ft2font.ppm") || die "cannot open testout/t38ft2font.ppm\n";
binmode(FH);
my $IO = Imager::io_new_fd(fileno(FH));
i_writeppm_wiol($overlay, $IO);
close(FH);

print "ok 2\n";

dotest:

$bgcolor=i_color_set($bgcolor,200,200,200,0);
$backgr=Imager::ImgRaw::new(500,300,3);

#     i_tt_set_aa(2);
Imager::Font::FreeType2::i_ft2_text($ttraw,$backgr,100,150,NC(255, 64, 64),200.0,50, 'MAW',1,1,0, 0);
Imager::Font::FreeType2::i_ft2_settransform($ttraw, [0.9659, 0.2588, 0, -0.2588, 0.9659, 0 ]);
Imager::Font::FreeType2::i_ft2_text($ttraw,$backgr,100,150,NC(0, 128, 0),200.0,50, 'MAW',0,1, 0, 0);
i_draw($backgr, 0,150, 499, 150, NC(0, 0, 255));

open(FH,">testout/t38ft2font2.ppm") || die "cannot open testout/t38ft2font.ppm\n";
binmode(FH);
$IO = Imager::io_new_fd(fileno(FH));
i_writeppm_wiol($backgr,$IO);
close(FH);

print "ok 3\n";

#$fontname = 'fontfiles/arial.ttf';
my $oof = Imager::Font->new(file=>$fontname, type=>'ft2', 'index'=>0)
  or print "not ";
print "ok 4\n";

my $im = Imager->new(xsize=>400, ysize=>250);

$im->string(font=>$oof,
            text=>"Via OO",
            'x'=>20,
            'y'=>20,
            size=>60,
            color=>NC(255, 128, 255),
            aa => 1,
            align=>0) or print "not ";
print "ok 5\n";
$oof->transform(matrix=>[1, 0.1, 0, 0, 1, 0])
  or print "not ";
print "ok 6\n";
$im->string(font=>$oof,
            text=>"Shear",
            'x'=>20,
            'y'=>40,
            size=>60,
            sizew=>50,
            channel=>1,
            aa=>1,
            align=>1) or print "not ";
print "ok 7\n";
use Imager::Matrix2d ':handy';
$oof->transform(matrix=>m2d_rotate(degrees=>-30));
#$oof->transform(matrix=>m2d_identity());
$im->string(font=>$oof,
            text=>"SPIN",
            'x'=>20,
            'y'=>50,
            size=>50,
  	    sizew=>40,
            color=>NC(255,255,0),
            aa => 1,
            align=>0, vlayout=>0)
and
$im->string(font=>$oof,
            text=>"SPIN",
            'x'=>20,
            'y'=>50,
            size=>50,
	    sizew=>40,
            channel=>2,
            aa => 1,
            align=>0, vlayout=>0) or print "not ";
print "ok 8\n";

$oof->transform(matrix=>m2d_identity());
$oof->hinting(hinting=>1);

# UTF8 testing
# the test font (dodge.ttf) only supports one character above 0xFF that
# I can see, 0x2010 HYPHEN (which renders the same as 0x002D HYPHEN MINUS)
# an attempt at utf8 support
# first attempt to use native perl UTF8
if ($] >= 5.006) {
  my $text;
  # we need to do this in eval to prevent compile time errors in older
  # versions
  eval q{$text = "A\x{2010}A"}; # A, HYPHEN, A in our test font
  #$text = "A".chr(0x2010)."A"; # this one works too
  if ($im->string(font=>$oof,
              text=>$text,
              'x'=>20,
              'y'=>200,
              size=>50,
              color=>NC(0,255,0),
              aa=>1)) {
    print "ok 9\n";
  }
  else {
    print "not ok 9 # ",$im->errstr,"\n";
  }
}
else {
  print "ok 9 # skip no native UTF8 support in this version of perl\n";
}

# an attempt using emulation of UTF8
my $text = pack("C*", 0x41, 0xE2, 0x80, 0x90, 0x41);
#my $text = "A\xE2\x80\x90\x41\x{2010}";
#substr($text, -1, 0) = '';
if ($im->string(font=>$oof,
                text=>$text,
                'x'=>20,
                'y'=>230,
                size=>50,
                color=>NC(255,128,0),
                aa=>1, 
                utf8=>1)) {
  print "ok 10\n";
}
else {
  print "not ok 10 # ",$im->errstr,"\n";
}

# just a bit of fun
# well it was - it demostrates what happens when you combine
# transformations and font hinting
for my $steps (0..39) {
  $oof->transform(matrix=>m2d_rotate(degrees=>-$steps+5));
  # demonstrates why we disable hinting on a doing a transform
  # if the following line is enabled then the 0 degrees output sticks 
  # out a bit
  # $oof->hinting(hinting=>1);
  $im->string(font=>$oof,
              text=>"SPIN",
              'x'=>160,
              'y'=>70,
              size=>65,
              color=>NC(255, $steps * 5, 200-$steps * 5),
              aa => 1,
              align=>0, ) or print "not ";
}

$im->write(file=>'testout/t38_oo.ppm')
  or print "# could not save OO output: ",$im->errstr,"\n";

my (@got) = $oof->has_chars(string=>"\x01H");
@got == 2 or print "not ";
print "ok 11\n";
$got[0] and print "not ";
print "ok 12 # check if \\x01 is defined\n";
$got[1] or print "not ";
print "ok 13 # check if 'H' is defined\n";
$oof->has_chars(string=>"H\x01") eq "\x01\x00" or print "not ";
print "ok 14\n";
