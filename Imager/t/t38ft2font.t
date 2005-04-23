#!perl -w
use strict;
use Test::More tests => 138;
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { use_ok(Imager => ':all') }

init_log("testout/t38ft2font.log",2);

my @base_color = (64, 255, 64);

SKIP:
{
  i_has_format("ft2") or skip("no freetype2 library found", 137);

  print "# has ft2\n";
  
  my $fontname=$ENV{'TTFONTTEST'}||'./fontfiles/dodge.ttf';

  -f $fontname or skip("cannot find fontfile $fontname", 137);


  my $bgcolor=i_color_new(255,0,0,0);
  my $overlay=Imager::ImgRaw::new(200,70,3);
  
  my $ttraw=Imager::Font::FreeType2::i_ft2_new($fontname, 0);
  
  $ttraw or print Imager::_error_as_msg(),"\n";
  ok($ttraw, "loaded raw font");

  my @bbox=Imager::Font::FreeType2::i_ft2_bbox($ttraw, 50.0, 0, 'XMCLH', 0);
  print "#bbox @bbox\n";
  
  is(@bbox, 7, "i_ft2_bbox() returns 7 values");

  ok(Imager::Font::FreeType2::i_ft2_cp($ttraw,$overlay,5,50,1,50.0,50, 'XMCLH',1,1, 0, 0), "drawn to channel");
  i_line($overlay,0,50,100,50,$bgcolor,1);

  open(FH,">testout/t38ft2font.ppm") || die "cannot open testout/t38ft2font.ppm\n";
  binmode(FH);
  my $IO = Imager::io_new_fd(fileno(FH));
  ok(i_writeppm_wiol($overlay, $IO), "saved image");
  close(FH);

  $bgcolor=i_color_set($bgcolor,200,200,200,0);
  my $backgr=Imager::ImgRaw::new(500,300,3);
  
  #     i_tt_set_aa(2);
  ok(Imager::Font::FreeType2::i_ft2_text($ttraw,$backgr,100,150,NC(255, 64, 64),200.0,50, 'MAW',1,1,0, 0), "drew MAW");
  Imager::Font::FreeType2::i_ft2_settransform($ttraw, [0.9659, 0.2588, 0, -0.2588, 0.9659, 0 ]);
  ok(Imager::Font::FreeType2::i_ft2_text($ttraw,$backgr,100,150,NC(0, 128, 0),200.0,50, 'MAW',0,1, 0, 0), "drew rotated MAW");
  i_line($backgr, 0,150, 499, 150, NC(0, 0, 255),1);

  open(FH,">testout/t38ft2font2.ppm") || die "cannot open testout/t38ft2font.ppm\n";
  binmode(FH);
  $IO = Imager::io_new_fd(fileno(FH));
  ok(i_writeppm_wiol($backgr,$IO), "saved second image");
  close(FH);

  my $oof = Imager::Font->new(file=>$fontname, type=>'ft2', 'index'=>0);

  ok($oof, "loaded OO font");

  my $im = Imager->new(xsize=>400, ysize=>250);
  
  ok($im->string(font=>$oof,
                 text=>"Via OO",
                 'x'=>20,
                 'y'=>20,
                 size=>60,
                 color=>NC(255, 128, 255),
                 aa => 1,
                 align=>0), "drawn through OO interface");
  ok($oof->transform(matrix=>[1, 0.1, 0, 0, 1, 0]),
     "set matrix via OO interface");
  ok($im->string(font=>$oof,
                 text=>"Shear",
                 'x'=>20,
                 'y'=>40,
                 size=>60,
                 sizew=>50,
                 channel=>1,
                 aa=>1,
                 align=>1), "drawn transformed through OO");
  use Imager::Matrix2d ':handy';
  ok($oof->transform(matrix=>m2d_rotate(degrees=>-30)),
     "set transform from m2d module");
  ok($im->string(font=>$oof,
                 text=>"SPIN",
                 'x'=>20,
                 'y'=>50,
                 size=>50,
                 sizew=>40,
                 color=>NC(255,255,0),
                 aa => 1,
                 align=>0, vlayout=>0), "drawn first rotated");

  ok($im->string(font=>$oof,
                 text=>"SPIN",
                 'x'=>20,
                 'y'=>50,
                 size=>50,
                 sizew=>40,
            channel=>2,
                 aa => 1,
                 align=>0, vlayout=>0), "drawn second rotated");
  
  $oof->transform(matrix=>m2d_identity());
  $oof->hinting(hinting=>1);

  # UTF8 testing
  # the test font (dodge.ttf) only supports one character above 0xFF that
  # I can see, 0x2010 HYPHEN (which renders the same as 0x002D HYPHEN MINUS)
  # an attempt at utf8 support
  # first attempt to use native perl UTF8
 SKIP:
  {
    skip("no native UTF8 support in this version of perl", 1) 
      unless $] >= 5.006;
    my $text;
    # we need to do this in eval to prevent compile time errors in older
    # versions
    eval q{$text = "A\x{2010}A"}; # A, HYPHEN, A in our test font
    #$text = "A".chr(0x2010)."A"; # this one works too
    unless (ok($im->string(font=>$oof,
                           text=>$text,
                           'x'=>20,
                           'y'=>200,
                           size=>50,
                           color=>NC(0,255,0),
                           aa=>1), "drawn UTF natively")) {
      print "# ",$im->errstr,"\n";
    }
  }

  # an attempt using emulation of UTF8
  my $text = pack("C*", 0x41, 0xE2, 0x80, 0x90, 0x41);
  #my $text = "A\xE2\x80\x90\x41\x{2010}";
  #substr($text, -1, 0) = '';
  unless (ok($im->string(font=>$oof,
                         text=>$text,
                         'x'=>20,
                         'y'=>230,
                         size=>50,
                         color=>NC(255,128,0),
                         aa=>1, 
                         utf8=>1), "drawn UTF emulated")) {
    print "# ",$im->errstr,"\n";
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
                align=>0, );
  }

  $im->write(file=>'testout/t38_oo.ppm')
    or print "# could not save OO output: ",$im->errstr,"\n";
  
  my (@got) = $oof->has_chars(string=>"\x01H");
  ok(@got == 2, "has_chars returned 2 items");
  ok(!$got[0], "have no chr(1)");
  ok($got[1], "have 'H'");
  ok($oof->has_chars(string=>"H\x01") eq "\x01\x00",
     "scalar has_chars()");

  print "# OO bounding boxes\n";
  @bbox = $oof->bounding_box(string=>"hello", size=>30);
  my $bbox = $oof->bounding_box(string=>"hello", size=>30);

  is(@bbox, 7, "list bbox returned 7 items");
  ok($bbox->isa('Imager::Font::BBox'), "scalar bbox returned right class");
  ok($bbox->start_offset == $bbox[0], "start_offset");
  ok($bbox->end_offset == $bbox[2], "end_offset");
  ok($bbox->global_ascent == $bbox[3], "global_ascent");
  ok($bbox->global_descent == $bbox[1], "global_descent");
  ok($bbox->ascent == $bbox[5], "ascent");
  ok($bbox->descent == $bbox[4], "descent");
  ok($bbox->advance_width == $bbox[6], "advance_width");

  print "# aligned text output\n";
  my $alimg = Imager->new(xsize=>300, ysize=>300);
  $alimg->box(color=>'40FF40', filled=>1);

  $oof->transform(matrix=>m2d_identity());
  $oof->hinting(hinting=>1);
  
  align_test('left', 'top', 10, 10, $oof, $alimg);
  align_test('start', 'top', 10, 40, $oof, $alimg);
  align_test('center', 'top', 150, 70, $oof, $alimg);
  align_test('end', 'top', 290, 100, $oof, $alimg);
  align_test('right', 'top', 290, 130, $oof, $alimg);

  align_test('center', 'top', 150, 160, $oof, $alimg);
  align_test('center', 'center', 150, 190, $oof, $alimg);
  align_test('center', 'bottom', 150, 220, $oof, $alimg);
  align_test('center', 'baseline', 150, 250, $oof, $alimg);
  
  ok($alimg->write(file=>'testout/t38aligned.ppm'), 
     "saving aligned output image");
  
  my $exfont = Imager::Font->new(file=>'fontfiles/ExistenceTest.ttf',
                                 type=>'ft2');
  SKIP:
  {
    ok($exfont, "loaded existence font") or
      skip("couldn't load test font", 5);
    # the test font is known to have a shorter advance width for that char
    my @bbox = $exfont->bounding_box(string=>"/", size=>100);
    is(@bbox, 7, "should be 7 entries");
    isnt($bbox[6], $bbox[2], "different advance width");
    my $bbox = $exfont->bounding_box(string=>"/", size=>100);
    ok($bbox->pos_width != $bbox->advance_width, "OO check");

    # name tests
    # make sure the number of tests on each branch match
    if (Imager::Font::FreeType2::i_ft2_can_face_name()) {
      my $facename = Imager::Font::FreeType2::i_ft2_face_name($exfont->{id});
      print "# face name '$facename'\n";
      ok($facename eq 'ExistenceTest', "test face name");
      $facename = $exfont->face_name;
      ok($facename eq 'ExistenceTest', "test face name OO");
    }
    else {
      # make sure we get the error we expect
      my $facename = Imager::Font::FreeType2::i_ft2_face_name($exfont->{id});
      my ($msg) = Imager::_error_as_msg();
      ok(!defined($facename), "test face name not supported");
      print "# $msg\n";
      ok(scalar($msg =~ /or later required/), "test face name not supported");
    }
  }

  SKIP:
  {
    Imager::Font::FreeType2->can_glyph_names
        or skip("FT2 compiled without glyph names support", 9);
        
    # FT2 considers POST tables in TTF fonts unreliable, so use
    # a type 1 font, see below for TTF test 
    my $exfont = Imager::Font->new(file=>'fontfiles/ExistenceTest.pfb',
                               type=>'ft2');
  SKIP:
    {
      ok($exfont, "load Type 1 via FT2")
        or skip("couldn't load type 1 with FT2", 8);
      my @glyph_names = 
        Imager::Font::FreeType2::i_ft2_glyph_name($exfont->{id}, "!J/");
      #use Data::Dumper;
      #print Dumper \@glyph_names;
      is($glyph_names[0], 'exclam', "check exclam name");
      ok(!defined($glyph_names[1]), "check for no J name");
      is($glyph_names[2], 'slash', "check slash name");

      # oo interfaces
      @glyph_names = $exfont->glyph_names(string=>"!J/");
      is($glyph_names[0], 'exclam', "check exclam name OO");
      ok(!defined($glyph_names[1]), "check for no J name OO");
      is($glyph_names[2], 'slash', "check slash name OO");

      # make sure a missing string parameter is handled correctly
      eval {
        $exfont->glyph_names();
      };
      is($@, "", "correct error handling");
      cmp_ok(Imager->errstr, '=~', qr/no string parameter/, "error message");
    }
  
    # freetype 2 considers truetype glyph name tables unreliable
    # due to some specific fonts, supplying reliable_only=>0 bypasses
    # that check and lets us get the glyph names even for truetype fonts
    # so we can test this stuff <sigh>
    # we can't use ExistenceTest.ttf since that's generated with 
    # AppleStandardEncoding since the same .sfd needs to generate
    # a .pfb file, NameTest.ttf uses a Unicode encoding
    
    # we were using an unsigned char to store a unicode character
    # https://rt.cpan.org/Ticket/Display.html?id=7949
    $exfont = Imager::Font->new(file=>'fontfiles/NameTest.ttf',
                                type=>'ft2');
  SKIP:
    {
      ok($exfont, "load TTF via FT2")
        or skip("could not load TTF with FT2", 1);
      my $text = pack("C*", 0xE2, 0x80, 0x90); # "\x{2010}" as utf-8
      my @names = $exfont->glyph_names(string=>$text,
                                       utf8=>1, reliable_only=>0);
      is($names[0], "hyphentwo", "check utf8 glyph name");
    }
  }

  # check that error codes are translated correctly
  my $errfont = Imager::Font->new(file=>"t/t38ft2font.t", type=>"ft2");
  is($errfont, undef, "new font vs non font");
  cmp_ok(Imager->errstr, '=~', qr/unknown file format/, "check error message");

  # Multiple Master tests
  # we check a non-MM font errors correctly
  print "# check that the methods act correctly for a non-MM font\n";
  ok(!$exfont->is_mm, "exfont not MM");
  ok((() = $exfont->mm_axes) == 0, "exfont has no MM axes");
  cmp_ok(Imager->errstr, '=~', qr/no multiple masters/, 
         "and returns correct error when we ask");
  ok(!$exfont->set_mm_coords(coords=>[0, 0]), "fail setting axis on exfont");
  cmp_ok(Imager->errstr, '=~', qr/no multiple masters/, 
         "and returns correct error when we ask");

  # try a MM font now - test font only has A defined
  print "# Try a multiple master font\n";
  my $mmfont = Imager::Font->new(file=>"fontfiles/MMOne.pfb", type=>"ft2", 
                                 color=>"white", aa=>1, size=>60);
  ok($mmfont, "loaded MM font");
  ok($mmfont->is_mm, "font is multiple master");
  my @axes = $mmfont->mm_axes;
  is(@axes, 2, "check we got both axes");
  is($axes[0][0], "Weight", "name of first axis");
  is($axes[0][1],  50, "min for first axis");
  is($axes[0][2], 999, "max for first axis");
  is($axes[1][0], "Slant", "name of second axis");
  is($axes[1][1],   0, "min for second axis");
  is($axes[1][2], 999, "max for second axis");
  my $mmim = Imager->new(xsize=>200, ysize=>200);
  $mmim->string(font=>$mmfont, x=>0, 'y'=>50, text=>"A");
  ok($mmfont->set_mm_coords(coords=>[ 700, 0 ]), "set to bold, unsloped");
  $mmim->string(font=>$mmfont, x=>0, 'y'=>100, text=>"A", color=>'blue');
  my @weights = qw(50 260 525 760 999);
  my @slants = qw(0 333 666 999);
  for my $windex (0 .. $#weights) {
    my $weight = $weights[$windex];
    for my $sindex (0 .. $#slants) {
      my $slant = $slants[$sindex];
      $mmfont->set_mm_coords(coords=>[ $weight, $slant ]);
      $mmim->string(font=>$mmfont, x=>30+32*$windex, 'y'=>50+45*$sindex,
                    text=>"A");
    }
  }

  ok($mmim->write(file=>"testout/t38mm.ppm"), "save MM output");
}

sub align_test {
  my ($h, $v, $x, $y, $f, $img) = @_;

  my @pos = $f->align(halign=>$h, valign=>$v, 'x'=>$x, 'y'=>$y,
                      image=>$img, size=>15, color=>'FFFFFF',
                      string=>"x$h ${v}y", channel=>1, aa=>1);
  @pos = $f->align(halign=>$h, valign=>$v, 'x'=>$x, 'y'=>$y,
                      image=>$img, size=>15, color=>'FF99FF',
                      string=>"x$h ${v}y", aa=>1);
  if (ok(@pos == 4, "$h $v aligned output")) {
    # checking corners
    my $cx = int(($pos[0] + $pos[2]) / 2);
    my $cy = int(($pos[1] + $pos[3]) / 2);
    
    print "# @pos cx $cx cy $cy\n";
    okmatchcolor($img, $cx, $pos[1]-1, @base_color, "outer top edge");
    okmatchcolor($img, $cx, $pos[3], @base_color, "outer bottom edge");
    okmatchcolor($img, $pos[0]-1, $cy, @base_color, "outer left edge");
    okmatchcolor($img, $pos[2], $cy, @base_color, "outer right edge");
    
    okmismatchcolor($img, $cx, $pos[1], @base_color, "inner top edge");
    okmismatchcolor($img, $cx, $pos[3]-1, @base_color, "inner bottom edge");
    okmismatchcolor($img, $pos[0], $cy, @base_color, "inner left edge");
#    okmismatchcolor($img, $pos[2]-1, $cy, @base_color, "inner right edge");
# XXX: This gets triggered by a freetype2 bug I think 
#    $ rpm -qa | grep freetype
#    freetype-2.1.3-6
#
# (addi: 4/1/2004).

    cross($img, $x, $y, 'FF0000');
    cross($img, $cx, $pos[1]-1, '0000FF');
    cross($img, $cx, $pos[3], '0000FF');
    cross($img, $pos[0]-1, $cy, '0000FF');
    cross($img, $pos[2], $cy, '0000FF');
  }
  else {
    SKIP: { skip("couldn't draw text", 8) };
  }
}

sub okmatchcolor {
  my ($img, $x, $y, $r, $g, $b, $about) = @_;

  my $c = $img->getpixel('x'=>$x, 'y'=>$y);
  my ($fr, $fg, $fb) = $c->rgba;
  ok($fr == $r && $fg == $g && $fb == $b,
      "want ($r,$g,$b) found ($fr,$fg,$fb)\@($x,$y) $about");
}

sub okmismatchcolor {
  my ($img, $x, $y, $r, $g, $b, $about) = @_;

  my $c = $img->getpixel('x'=>$x, 'y'=>$y);
  my ($fr, $fg, $fb) = $c->rgba;
  ok($fr != $r || $fg != $g || $fb != $b,
      "don't want ($r,$g,$b) found ($fr,$fg,$fb)\@($x,$y) $about");
}

sub cross {
  my ($img, $x, $y, $color) = @_;

  $img->setpixel('x'=>[$x, $x, $x, $x, $x, $x-2, $x-1, $x+1, $x+2], 
                 'y'=>[$y-2, $y-1, $y, $y+1, $y+2, $y, $y, $y, $y], 
                 color => $color);
  
}