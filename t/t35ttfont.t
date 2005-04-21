#!perl -w
use strict;
use lib 't';
use Test::More tests => 40;

BEGIN { use_ok(Imager => ':all') }
require "t/testtools.pl";

init_log("testout/t35ttfont.log",2);

SKIP:
{
  skip("freetype 1.x unavailable or disabled", 39) 
    unless i_has_format("tt");
  print "# has tt\n";
  
  my $deffont = './fontfiles/dodge.ttf';
  my $fontname=$ENV{'TTFONTTEST'} || $deffont;

  if (!ok(-f $fontname, "check test font file exists")) {
    print "# cannot find fontfile for truetype test $fontname\n";
    skip('Cannot load test font', 38);
  }

  i_init_fonts();
  #     i_tt_set_aa(1);
  
  my $bgcolor = i_color_new(255,0,0,0);
  my $overlay = Imager::ImgRaw::new(200,70,3);
  
  my $ttraw = Imager::i_tt_new($fontname);
  ok($ttraw, "create font");

  my @bbox = i_tt_bbox($ttraw,50.0,'XMCLH',5,0);
  is(@bbox, 7, "bounding box");
  print "#bbox: ($bbox[0], $bbox[1]) - ($bbox[2], $bbox[3])\n";

  ok(i_tt_cp($ttraw,$overlay,5,50,1,50.0,'XMCLH',5,1,0), "cp output");
  i_line($overlay,0,50,100,50,$bgcolor,1);

  open(FH,">testout/t35ttfont.ppm") || die "cannot open testout/t35ttfont.ppm\n";
  binmode(FH);
  my $IO = Imager::io_new_fd( fileno(FH) );
  ok(i_writeppm_wiol($overlay, $IO), "save t35ttfont.ppm");
  close(FH);

  $bgcolor=i_color_set($bgcolor,200,200,200,0);
  my $backgr=Imager::ImgRaw::new(500,300,3);
  
  #     i_tt_set_aa(2);
  
  ok(i_tt_text($ttraw,$backgr,100,120,$bgcolor,50.0,'test',4,1,0),
      "normal output");

  my $ugly = Imager::i_tt_new("./fontfiles/ImUgly.ttf");
  ok($ugly, "create ugly font");
  # older versions were dropping the bottom of g and the right of a
  ok(i_tt_text($ugly, $backgr,100, 80, $bgcolor, 14, 'g%g', 3, 1, 0), 
     "draw g%g");
  ok(i_tt_text($ugly, $backgr,150, 80, $bgcolor, 14, 'delta', 5, 1, 0),
      "draw delta");
  i_line($backgr,0,20,499,20,i_color_new(0,127,0,0),1);
  ok(i_tt_text($ttraw, $backgr, 20, 20, $bgcolor, 14, 'abcdefghijklmnopqrstuvwxyz{|}', 29, 1, 0), "alphabet");
  ok(i_tt_text($ttraw, $backgr, 20, 50, $bgcolor, 14, 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 26, 1, 0), "ALPHABET");
  
  # UTF8 tests
  # for perl < 5.6 we can hand-encode text
  # the following is "A\x{2010}A"
  # 
  my $text = pack("C*", 0x41, 0xE2, 0x80, 0x90, 0x41);
  my $alttext = "A-A";
  
  my @utf8box = i_tt_bbox($ttraw, 50.0, $text, length($text), 1);
  is(@utf8box, 7, "utf8 bbox element count");
  my @base = i_tt_bbox($ttraw, 50.0, $alttext, length($alttext), 0);
  is(@base, 7, "alt bbox element count");
  my $maxdiff = $fontname eq $deffont ? 0 : $base[2] / 3;
  print "# (@utf8box vs @base)\n";
  ok(abs($utf8box[2] - $base[2]) <= $maxdiff, 
     "compare box sizes $utf8box[2] vs $base[2] (maxerror $maxdiff)");
  
  # hand-encoded UTF8 drawing
  ok(i_tt_text($ttraw, $backgr, 200, 80, $bgcolor, 14, $text, length($text), 1, 1), "draw hand-encoded UTF8");

  ok(i_tt_cp($ttraw, $backgr, 250, 80, 1, 14, $text, length($text), 1, 1), 
      "cp hand-encoded UTF8");

  # ok, try native perl UTF8 if available
 SKIP:
  {
    skip("perl too old to test native UTF8 support", 5) unless $] >= 5.006;

    my $text;
    # we need to do this in eval to prevent compile time errors in older
    # versions
    eval q{$text = "A\x{2010}A"}; # A, HYPHEN, A in our test font
    #$text = "A".chr(0x2010)."A"; # this one works too
    ok(i_tt_text($ttraw, $backgr, 300, 80, $bgcolor, 14, $text, 0, 1, 0),
       "draw UTF8");
    ok(i_tt_cp($ttraw, $backgr, 350, 80, 0, 14, $text, 0, 1, 0),
       "cp UTF8");
    @utf8box = i_tt_bbox($ttraw, 50.0, $text, length($text), 0);
    is(@utf8box, 7, "native utf8 bbox element count");
    ok(abs($utf8box[2] - $base[2]) <= $maxdiff, 
       "compare box sizes native $utf8box[2] vs $base[2] (maxerror $maxdiff)");
    eval q{$text = "A\x{0905}\x{0906}\x{0103}A"}; # Devanagari
    ok(i_tt_text($ugly, $backgr, 100, 160, $bgcolor, 36, $text, 0, 1, 0),
       "more complex output");
  }

  open(FH,">testout/t35ttfont2.ppm") || die "cannot open testout/t35ttfont.ppm\n";
  binmode(FH);
  $IO = Imager::io_new_fd( fileno(FH) );
  ok(i_writeppm_wiol($backgr, $IO), "save t35ttfont2.ppm");
  close(FH);
  
  my $exists_font = "fontfiles/ExistenceTest.ttf";
  my $hcfont = Imager::Font->new(file=>$exists_font, type=>'tt');
 SKIP:
  {
    ok($hcfont, "loading existence test font")
      or skip("could not load test font", 11);

    # list interface
    my @exists = $hcfont->has_chars(string=>'!A');
    ok(@exists == 2, "check return count");
    ok($exists[0], "we have an exclamation mark");
    ok(!$exists[1], "we have no exclamation mark");
    
    # scalar interface
    my $exists = $hcfont->has_chars(string=>'!A');
    ok(length($exists) == 2, "check return length");
    ok(ord(substr($exists, 0, 1)), "we have an exclamation mark");
    ok(!ord(substr($exists, 1, 1)), "we have no upper-case A");
    
    my $face_name = Imager::i_tt_face_name($hcfont->{id});
    print "# face $face_name\n";
    ok($face_name eq 'ExistenceTest', "face name");
    $face_name = $hcfont->face_name;
    ok($face_name eq 'ExistenceTest', "face name");
    
    # FT 1.x cheats and gives names even if the font doesn't have them
    my @glyph_names = $hcfont->glyph_names(string=>"!J/");
    ok($glyph_names[0] eq 'exclam', "check exclam name OO");
    ok(!defined($glyph_names[1]), "check for no J name OO");
    ok($glyph_names[2] eq 'slash', "check slash name OO");
    
    print "# ** name table of the test font **\n";
    Imager::i_tt_dump_names($hcfont->{id});
  }
  undef $hcfont;
  
  my $name_font = "fontfiles/NameTest.ttf";
  $hcfont = Imager::Font->new(file=>$name_font);
 SKIP:
  {
    ok($hcfont, "loading name font")
      or skip("could not load name font $name_font", 3);
    # make sure a missing string parameter is handled correctly
    eval {
      $hcfont->glyph_names();
    };
    is($@, "", "correct error handling");
    cmp_ok(Imager->errstr, '=~', qr/no string parameter/, "error message");
    
    my $text = pack("C*", 0xE2, 0x80, 0x90); # "\x{2010}" as utf-8
    my @names = $hcfont->glyph_names(string=>$text, utf8=>1);
    is($names[0], "hyphentwo", "check utf8 glyph name");
  }

  undef $hcfont;
  
  ok(1, "end of code");
}
