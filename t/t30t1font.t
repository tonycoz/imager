#!perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)
use strict;
my $loaded;
BEGIN { $| = 1; print "1..41\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager qw(:all);
use Imager::Color;
require "t/testtools.pl";

$loaded = 1;
okx(1, "loaded");

#$Imager::DEBUG=1;

init_log("testout/t30t1font.log",1);

my $deffont = './fontfiles/dcr10.pfb';

my $fontname_pfb=$ENV{'T1FONTTESTPFB'}||$deffont;
my $fontname_afm=$ENV{'T1FONTTESTAFM'}||'./fontfiles/dcr10.afm';


if (!(i_has_format("t1")) ) {
  skipx(40, "t1lib unavailable or disabled");
}
elsif (! -f $fontname_pfb) {
  skipx(40, "cannot find fontfile for type 1 test $fontname_pfb");
}
elsif (! -f $fontname_afm) {
  skipx(40, "cannot find fontfile for type 1 test $fontname_afm");
} else {

  print "# has t1\n";

  i_t1_set_aa(1);

  my $fnum=Imager::i_t1_new($fontname_pfb,$fontname_afm); # this will load the pfb font
  unless (okx($fnum >= 0, "load font $fontname_pfb")) {
    skipx(31, "without the font I can't do a thing");
    exit;
  }

  my $bgcolor=Imager::Color->new(255,0,0,0);
  my $overlay=Imager::ImgRaw::new(200,70,3);
  
  okx(i_t1_cp($overlay,5,50,1,$fnum,50.0,'XMCLH',5,1), "i_t1_cp");

  i_line($overlay,0,50,100,50,$bgcolor,1);

  my @bbox=i_t1_bbox(0,50.0,'XMCLH',5);
  okx(@bbox == 7, "i_t1_bbox");
  print "# bbox: ($bbox[0], $bbox[1]) - ($bbox[2], $bbox[3])\n";

  open(FH,">testout/t30t1font.ppm") || die "cannot open testout/t35t1font.ppm\n";
  binmode(FH); # for os2
  my $IO = Imager::io_new_fd( fileno(FH) );
  i_writeppm_wiol($overlay,$IO);
  close(FH);

  $bgcolor=Imager::Color::set($bgcolor,200,200,200,0);
  my $backgr=Imager::ImgRaw::new(280,300,3);

  i_t1_set_aa(2);
  okx(i_t1_text($backgr,10,100,$bgcolor,$fnum,150.0,'test',4,1), "i_t1_text");

  # "UTF8" tests
  # for perl < 5.6 we can hand-encode text
  # since T1 doesn't support over 256 chars in an encoding we just drop
  # chars over \xFF
  # the following is "A\xA1\x{2010}A"
  # 
  my $text = pack("C*", 0x41, 0xC2, 0xA1, 0xE2, 0x80, 0x90, 0x41);
  my $alttext = "A\xA1A";
  
  my @utf8box = i_t1_bbox($fnum, 50.0, $text, length($text), 1);
  okx(@utf8box == 7, "utf8 bbox element count");
  my @base = i_t1_bbox($fnum, 50.0, $alttext, length($alttext), 0);
  okx(@base == 7, "alt bbox element count");
  my $maxdiff = $fontname_pfb eq $deffont ? 0 : $base[2] / 3;
  print "# (@utf8box vs @base)\n";
  okx(abs($utf8box[2] - $base[2]) <= $maxdiff, 
      "compare box sizes $utf8box[2] vs $base[2] (maxerror $maxdiff)");

  # hand-encoded UTF8 drawing
  okx(i_t1_text($backgr, 10, 140, $bgcolor, $fnum, 32, $text, length($text), 1,1), "draw hand-encoded UTF8");

  okx(i_t1_cp($backgr, 80, 140, 1, $fnum, 32, $text, length($text), 1, 1), 
      "cp hand-encoded UTF8");

  # ok, try native perl UTF8 if available
  if ($] >= 5.006) {
    my $text;
    # we need to do this in eval to prevent compile time errors in older
    # versions
    eval q{$text = "A\xA1\x{2010}A"}; # A, a with ogonek, HYPHEN, A in our test font
    #$text = "A".chr(0xA1).chr(0x2010)."A"; # this one works too
    okx(i_t1_text($backgr, 10, 180, $bgcolor, $fnum, 32, $text, length($text), 1),
        "draw UTF8");
    okx(i_t1_cp($backgr, 80, 180, 1, $fnum, 32, $text, length($text), 1),
        "cp UTF8");
    @utf8box = i_t1_bbox($fnum, 50.0, $text, length($text), 0);
    okx(@utf8box == 7, "native utf8 bbox element count");
    okx(abs($utf8box[2] - $base[2]) <= $maxdiff, 
      "compare box sizes native $utf8box[2] vs $base[2] (maxerror $maxdiff)");
    eval q{$text = "A\xA1\xA2\x01\x1F\x{0100}A"};
    okx(i_t1_text($backgr, 10, 220, $bgcolor, $fnum, 32, $text, 0, 1, 0, "uso"),
        "more complex output");
  }
  else {
    skipx(5, "perl too old to test native UTF8 support");
  }

  open(FH,">testout/t30t1font2.ppm") || die "cannot open testout/t35t1font.ppm\n";
  binmode(FH);
  $IO = Imager::io_new_fd( fileno(FH) );
  i_writeppm_wiol($backgr, $IO);
  close(FH);

  my $rc=i_t1_destroy($fnum);
  unless (okx($rc >= 0, "i_t1_destroy")) {
    print "# i_t1_destroy failed: rc=$rc\n";
  }

  print "# debug: ",join(" x ",i_t1_bbox(0,50,"eses",4) ),"\n";
  print "# debug: ",join(" x ",i_t1_bbox(0,50,"llll",4) ),"\n";

  unlink "t1lib.log"; # lose it if it exists
  init(t1log=>0);
  okx(!-e("t1lib.log"), "disable t1log");
  init(t1log=>1);
  okx(-e("t1lib.log"), "enable t1log");
  init(t1log=>0);
  unlink "t1lib.log";

  # character existance tests - uses the special ExistenceTest font
  my $exists_font = 'fontfiles/ExistenceTest.pfb';
  my $exists_afm = 'fontfiles/ExistenceText.afm';
  
  -e $exists_font or die;
    
  my $font_num = Imager::i_t1_new($exists_font, $exists_afm);
  if (okx($font_num >= 0, 'loading test font')) {
    # first the list interface
    my @exists = Imager::i_t1_has_chars($font_num, "!A");
    okx(@exists == 2, "return count from has_chars");
    okx($exists[0], "we have an exclamation mark");
    okx(!$exists[1], "we have no uppercase A");

    # then the scalar interface
    my $exists = Imager::i_t1_has_chars($font_num, "!A");
    okx(length($exists) == 2, "return scalar length");
    okx(ord(substr($exists, 0, 1)), "we have an exclamation mark");
    okx(!ord(substr($exists, 1, 1)), "we have no upper-case A");
  }
  else {
    skipx(6, 'Could not load test font');
  }
  
  my $font = Imager::Font->new(file=>$exists_font, type=>'t1');
  if (okx($font, "loaded OO font")) {
    my @exists = $font->has_chars(string=>"!A");
    okx(@exists == 2, "return count from has_chars");
    okx($exists[0], "we have an exclamation mark");
    okx(!$exists[1], "we have no uppercase A");
    
    # then the scalar interface
    my $exists = $font->has_chars(string=>"!A");
    okx(length($exists) == 2, "return scalar length");
    okx(ord(substr($exists, 0, 1)), "we have an exclamation mark");
    okx(!ord(substr($exists, 1, 1)), "we have no upper-case A");

    # check the advance width
    my @bbox = $font->bounding_box(string=>'/', size=>100);
    print "# @bbox\n";
    okx($bbox[2] != $bbox[5], "different advance to pos_width");

    # names
    my $face_name = Imager::i_t1_face_name($font->{id});
    print "# face $face_name\n";
    okx($face_name eq 'ExistenceTest', "face name");
    $face_name = $font->face_name;
    okx($face_name eq 'ExistenceTest', "face name");

    my @glyph_names = $font->glyph_names(string=>"!J/");
    isx($glyph_names[0], 'exclam', "check exclam name OO");
    okx(!defined($glyph_names[1]), "check for no J name OO");
    isx($glyph_names[2], 'slash', "check slash name OO");

    # this character chosen since when it's truncated to one byte it
    # becomes 0x21 or '!' which the font does define
    my $text = pack("C*", 0xE2, 0x80, 0xA1); # "\x{2021}" as utf-8
    @glyph_names = $font->glyph_names(string=>$text, utf8=>1);
    isx($glyph_names[0], undef, "expect no glyph_name for \\x{20A1}");

    # make sure a missing string parameter is handled correctly
    eval {
      $font->glyph_names();
    };
    isx($@, "", "correct error handling");
    matchx(Imager->errstr, qr/no string parameter/, "error message");
  }
  else {
    skipx(15, "Could not load test font");
  }
}

#malloc_state();
