# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)
use lib qw(blib/lib blib/arch);

BEGIN { $| = 1; print "1..7\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager qw(:all);

$loaded = 1;
print "ok 1\n";

init_log("testout/t102png.log",1);

i_has_format("png") && print "# has png\n";

$green  = i_color_new(0,   255, 0,   255);
$blue   = i_color_new(0,   0,   255, 255);
$red    = i_color_new(255, 0,   0,   255);

$img    = Imager::ImgRaw::new(150, 150, 3);
$cmpimg = Imager::ImgRaw::new(150, 150, 3);

i_box_filled($img, 70, 25, 130, 125, $green);
i_box_filled($img, 20, 25, 80,  125, $blue);
i_arc($img, 75, 75, 30, 0, 361, $red);
i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

my $timg = Imager::ImgRaw::new(20, 20, 4);
my $trans = i_color_new(255, 0, 0, 127);
i_box_filled($timg, 0, 0, 20, 20, $green);
i_box_filled($timg, 2, 2, 18, 18, $trans);

if (!i_has_format("png")) {
  print "ok 2 # skip no png support\n";
  print "ok 3 # skip no png support\n";
  print "ok 4 # skip no png support\n";
  print "ok 5 # skip no png support\n";
} else {
  open(FH,">testout/t12.png") || die "cannot open testout/t12.png for writing\n";
  binmode(FH);
  i_writepng($img,fileno(FH)) || print "not ";
  close(FH);

  print "ok 2\n";

  open(FH,"testout/t12.png") || die "cannot open testout/t12.png\n";
  binmode(FH);
  $cmpimg=i_readpng(fileno(FH)) || print "not ";
  close(FH);

  print "ok 3\n";
  print "# png average mean square pixel difference: ",sqrt(i_img_diff($img,$cmpimg))/150*150,"\n";
  print i_img_diff($img, $cmpimg)
    ? "not ok 4 # saved image different\n" : "ok 4\n";

  open FH, "> testout/t12_trans.png"
    or die "Cannot open testout/t12_trans.png: $!";
  binmode FH;
  if (i_writepng($timg, fileno(FH))) {
    print "ok 5\n";
  }
  else {
    print "ok 5 # skip - png transparency not yet implemented\n";
  }
  close FH;

  open FH,"testout/t12_trans.png" 
    or die "cannot open testout/t12_trans.png\n";
  binmode(FH);
  $cmpimg=i_readpng(fileno(FH)) || print "not ";
  close(FH);

  print "ok 6\n";
  print "# png average mean square pixel difference: ",sqrt(i_img_diff($timg,$cmpimg))/150*150,"\n";
  print i_img_diff($timg, $cmpimg)
	? "not ok 7 # saved image different\n" : "ok 7\n";
}
