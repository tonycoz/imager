#!perl -w
use strict;
use lib 't';
use Test::More tests => 7;
BEGIN { use_ok(Imager => ':all') }

init_log("testout/t37w32font.log",1);

SKIP:
{
  i_has_format('w32') or skip("no MS Windows", 6);
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
  is(@bbox2, 6, "got 6 values from bounding_box");

  # this only applies while the Win32 driver returns 6 values
  # at this point we don't return the advance width from the low level
  # bounding box function, so the Imager::Font::BBox advance method should
  # return end_offset, check it does
  my $bbox = $font->bounding_box(string=>"some text");
  ok($bbox, "got the bounding box object");
  is($bbox->advance_width, $bbox->end_offset, 
     "check advance_width fallback correct");
}
