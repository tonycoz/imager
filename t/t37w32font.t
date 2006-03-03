#!perl -w
use strict;
use lib 't';
use Test::More tests => 32;
BEGIN { use_ok(Imager => ':all') }
++$|;

init_log("testout/t37w32font.log",1);

SKIP:
{
  i_has_format('w32') or skip("no MS Windows", 31);
  print "# has w32\n";

  my $fontname=$ENV{'TTFONTTEST'} || 'Times New Roman Bold';
  
  # i_init_fonts(); # unnecessary for Win32 font support

  my $bgcolor=i_color_new(255,0,0,0);
  my $overlay=Imager::ImgRaw::new(200,70,3);
  
  my @bbox=Imager::i_wf_bbox($fontname, 50.0,'XMCLH');
  print "#bbox: ($bbox[0], $bbox[1]) - ($bbox[2], $bbox[3])\n";
  
  ok(Imager::i_wf_cp($fontname,$overlay,5,50,1,50.0,'XMCLH',1,1),
     "i_wf_cp smoke test");
  i_line($overlay,0,50,100,50,$bgcolor, 1);
  
  open(FH,">testout/t37w32font.ppm") || die "cannot open testout/t37w32font.ppm\n";
  binmode(FH);
  my $io = Imager::io_new_fd(fileno(FH));
  i_writeppm_wiol($overlay,$io);
  close(FH);
  
  $bgcolor=i_color_set($bgcolor,200,200,200,0);
  my $backgr=Imager::ImgRaw::new(500,300,3);
  
  ok(Imager::i_wf_text($fontname,$backgr,100,100,$bgcolor,100,'MAW.',1, 1),
     "i_wf_text smoke test");
  i_line($backgr,0, 100, 499, 100, NC(0, 0, 255), 1);
  
  open(FH,">testout/t37w32font2.ppm") || die "cannot open testout/t37w32font2.ppm\n";
  binmode(FH);
  $io = Imager::io_new_fd(fileno(FH));
  i_writeppm_wiol($backgr,$io);
  close(FH);
  
  my $img = Imager->new(xsize=>200, ysize=>200);
  my $font = Imager::Font->new(face=>$fontname, size=>20);
  ok($img->string('x'=>30, 'y'=>30, string=>"Imager", color=>NC(255, 0, 0), 
	       font=>$font),
     "string with win32 smoke test")
    or print "# ",$img->errstr,"\n";
  $img->write(file=>'testout/t37_oo.ppm') or print "not ";
  my @bbox2 = $font->bounding_box(string=>'Imager');
  is(@bbox2, 8, "got 8 values from bounding_box");

  # this only applies while the Win32 driver returns 6 values
  # at this point we don't return the advance width from the low level
  # bounding box function, so the Imager::Font::BBox advance method should
  # return end_offset, check it does
  my $bbox = $font->bounding_box(string=>"some text");
  ok($bbox, "got the bounding box object");
  is($bbox->advance_width, $bbox->end_offset, 
     "check advance_width fallback correct");

 SKIP:
  {
    $^O eq 'cygwin' and skip("Too hard to get correct directory for test font on cygwin", 11);
    ok(Imager::i_wf_addfont("fontfiles/ExistenceTest.ttf"), "add test font")
      or print "# ",Imager::_error_as_msg(),"\n";
    
    my $namefont = Imager::Font->new(face=>"ExistenceTest");
    ok($namefont, "create font based on added font");
    
    # the test font is known to have a shorter advance width for that char
    @bbox = $namefont->bounding_box(string=>"/", size=>100);
    print "# / box: @bbox\n";
    is(@bbox, 8, "should be 8 entries");
    isnt($bbox[6], $bbox[2], "different advance width");
    $bbox = $namefont->bounding_box(string=>"/", size=>100);
    ok($bbox->pos_width != $bbox->advance_width, "OO check");
    
    cmp_ok($bbox->right_bearing, '<', 0, "check right bearing");
  
    cmp_ok($bbox->display_width, '>', $bbox->advance_width,
	   "check display width (roughly)");
    
    my $im = Imager->new(xsize=>200, ysize=>200);
    $im->string(font=>$namefont, text=>"/", x=>20, y=>100, color=>'white', size=>100);
    $im->line(color=>'blue', x1=>20, y1=>0, x2=>20, y2=>199);
    my $right = 20 + $bbox->advance_width;
    $im->line(color=>'blue', x1=>$right, y1=>0, x2=>$right, y2=>199);
    $im->write(file=>'testout/t37w32_slash.ppm');
    
    # check with a char that fits inside the box
    $bbox = $namefont->bounding_box(string=>"!", size=>100);
    print "# pos width ", $bbox->pos_width, "\n";
    print "# ! box: @$bbox\n";
    is($bbox->pos_width, $bbox->advance_width, 
     "check backwards compatibility");
    cmp_ok($bbox->left_bearing, '>', 0, "left bearing positive");
    cmp_ok($bbox->right_bearing, '>', 0, "right bearing positive");
    cmp_ok($bbox->display_width, '<', $bbox->advance_width,
	   "display smaller than advance");
  }

 SKIP:
  { print "# alignment tests\n";
    my $font = Imager::Font->new(face=>"Arial");
    ok($font, "loaded Arial OO")
      or skip("could not load font:".Imager->errstr, 4);
    my $im = Imager->new(xsize=>140, ysize=>150);
    my %common = 
      (
       font=>$font, 
       size=>40, 
       aa=>1,
      );
    $im->line(x1=>0, y1=>40, x2=>139, y2=>40, color=>'blue');
    $im->line(x1=>0, y1=>90, x2=>139, y2=>90, color=>'blue');
    $im->line(x1=>0, y1=>110, x2=>139, y2=>110, color=>'blue');
    for my $args ([ x=>5,   text=>"A", color=>"white" ],
                  [ x=>40,  text=>"y", color=>"white" ],
                  [ x=>75,  text=>"A", channel=>1 ],
                  [ x=>110, text=>"y", channel=>1 ]) {
      ok($im->string(%common, @$args, 'y'=>40), "A no alignment");
      ok($im->string(%common, @$args, 'y'=>90, align=>1), "A align=1");
      ok($im->string(%common, @$args, 'y'=>110, align=>0), "A align=0");
    }
    ok($im->write(file=>'testout/t37align.ppm'), "save align image");
  }

}
