use Imager ':all';

print "1..6\n";

init_log("testout/t104ppm.log",1);

$green=i_color_new(0,255,0,255);
$blue=i_color_new(0,0,255,255);
$red=i_color_new(255,0,0,255);

$img=Imager::ImgRaw::new(150,150,3);
$cmpimg=Imager::ImgRaw::new(150,150,3);

i_box_filled($img,70,25,130,125,$green);
i_box_filled($img,20,25,80,125,$blue);
i_arc($img,75,75,30,0,361,$red);
i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

my $timg = Imager::ImgRaw::new(20, 20, 4);
my $trans = i_color_new(255, 0, 0, 127);
i_box_filled($timg, 0, 0, 20, 20, $green);
i_box_filled($timg, 2, 2, 18, 18, $trans);

open(FH,">testout/t104.ppm") || die "Cannot open testout/t104.ppm\n";
binmode(FH);
i_writeppm($img,fileno(FH)) || die "Cannot write testout/t104.ppm\n";
close(FH);

print "ok 1\n";

open(FH,"testout/t104.ppm") || die "Cannot open testout/t104.ppm\n";
binmode(FH);

my $IO = Imager::io_new_fd(fileno(FH));
$cmpimg=i_readpnm_wiol($IO,-1) || die "Cannot read testout/t104.ppm\n";
close(FH);

print "ok 2\n";

print i_img_diff($img, $cmpimg) 
  ? "not ok 3 # saved image different\n" : "ok 3\n";

# build a grayscale image
my $gimg = Imager::ImgRaw::new(150, 150, 1);
my $gray = i_color_new(128, 0, 0, 255);
my $dgray = i_color_new(64, 0, 0, 255);
my $white = i_color_new(255, 0, 0, 255);
i_box_filled($gimg, 20, 20, 130, 130, $gray);
i_box_filled($gimg, 40, 40, 110, 110, $dgray);
i_arc($gimg, 75, 75, 30, 0, 361, $white);
open FH, "> testout/t104_gray.pgm"
  or die "Cannot create testout/t104_gray.pgm: $!\n";
binmode FH;
i_writeppm($gimg, fileno(FH))
  or print "not ";
print "ok 4\n";
close FH;
open FH, "< testout/t104_gray.pgm"
  or die "Cannot open testout/t104_gray.pgm: $!\n";
binmode FH;
$IO = Imager::io_new_fd(fileno(FH));
my $gcmpimg = i_readpnm_wiol($IO, -1) 
  or print "not ";
print "ok 5\n";
i_img_diff($gimg, $gcmpimg) == 0 or print "not ";
print "ok 6\n";

