# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..3\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager qw(:all);
$loaded = 1;
print "ok 1\n";

init_log("testout/t35ttfont.log",1);

sub skip { 
  print "ok 2 # skip\n";
  print "ok 3 # skip\n";
  malloc_state();
  exit(0);
}

if (!(i_has_format("tt")) ) { skip(); } 
print "# has tt\n";

$fontname=$ENV{'TTFONTTEST'}||'./fontfiles/dodge.ttf';

if (! -f $fontname) {
  print "# cannot find fontfile for truetype test $fontname\n";
  skip();	
}

i_init_fonts();
#     i_tt_set_aa(1);

$bgcolor = i_color_new(255,0,0,0);
$overlay = Imager::ImgRaw::new(200,70,3);

$ttraw = Imager::i_tt_new($fontname);

#use Data::Dumper;
#warn Dumper($ttraw);

@bbox = i_tt_bbox($ttraw,50.0,'XMCLH',5);
print "#bbox: ($bbox[0], $bbox[1]) - ($bbox[2], $bbox[3])\n";

i_tt_cp($ttraw,$overlay,5,50,1,50.0,'XMCLH',5,1);
i_draw($overlay,0,50,100,50,$bgcolor);

open(FH,">testout/t35ttfont.ppm") || die "cannot open testout/t35ttfont.ppm\n";
binmode(FH);
$IO = Imager::io_new_fd( fileno(FH) );
i_writeppm_wiol($overlay, $IO);
close(FH);

print "ok 2\n";

$bgcolor=i_color_set($bgcolor,200,200,200,0);
$backgr=Imager::ImgRaw::new(500,300,3);

#     i_tt_set_aa(2);

i_tt_text($ttraw,$backgr,100,100,$bgcolor,50.0,'test',4,1);

my $ugly = Imager::i_tt_new("./fontfiles/ImUgly.ttf");
i_tt_text($ugly, $backgr,100, 50, $bgcolor, 14, 'g%g', 3, 1);
i_tt_text($ugly, $backgr,150, 50, $bgcolor, 14, 'delta', 5, 1);


open(FH,">testout/t35ttfont2.ppm") || die "cannot open testout/t35ttfont.ppm\n";
binmode(FH);
$IO = Imager::io_new_fd( fileno(FH) );
i_writeppm_wiol($backgr, $IO);
close(FH);

print "ok 3\n";

