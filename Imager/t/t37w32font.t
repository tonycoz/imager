#!perl -w
BEGIN { $| = 1; print "1..5\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager qw(:all);
$loaded = 1;
print "ok 1\n";

init_log("testout/t37w32font.log",1);

sub skip { 
  for (2..5) {
    print "ok $_ # skip not MS Windows\n";
  }
  malloc_state();
  exit(0);
}

i_has_format('w32') or skip();
print "# has w32\n";

$fontname=$ENV{'TTFONTTEST'} || 'Times New Roman Bold';

# i_init_fonts(); # unnecessary for Win32 font support

$bgcolor=i_color_new(255,0,0,0);
$overlay=Imager::ImgRaw::new(200,70,3);

@bbox=Imager::i_wf_bbox($fontname, 50.0,'XMCLH');
print "#bbox: ($bbox[0], $bbox[1]) - ($bbox[2], $bbox[3])\n";

Imager::i_wf_cp($fontname,$overlay,5,50,1,50.0,'XMCLH',1,1);
i_line($overlay,0,50,100,50,$bgcolor, 1);

open(FH,">testout/t37w32font.ppm") || die "cannot open testout/t37w32font.ppm\n";
binmode(FH);
$io = Imager::io_new_fd(fileno(FH));
i_writeppm_wiol($overlay,$io);
close(FH);

print "ok 2\n";

$bgcolor=i_color_set($bgcolor,200,200,200,0);
$backgr=Imager::ImgRaw::new(500,300,3);

Imager::i_wf_text($fontname,$backgr,100,100,$bgcolor,100,'MAW.',1, 1);
i_line($backgr,0, 100, 499, 100, NC(0, 0, 255), 1);

open(FH,">testout/t37w32font2.ppm") || die "cannot open testout/t37w32font2.ppm\n";
binmode(FH);
$io = Imager::io_new_fd(fileno(FH));
i_writeppm_wiol($backgr,$io);
close(FH);

print "ok 3\n";

my $img = Imager->new(xsize=>200, ysize=>200);
my $font = Imager::Font->new(face=>$fontname, size=>20);
$img->string('x'=>30, 'y'=>30, string=>"Imager", color=>NC(255, 0, 0), 
	     font=>$font);
$img->write(file=>'testout/t37_oo.ppm') or print "not ";
print "ok 4 # ",$img->errstr||'',"\n";
my @bbox2 = $font->bounding_box(string=>'Imager');
if (@bbox2 == 6) {
  print "ok 5 # @bbox2\n";
}
else {
  print "not ok 5\n";
}
