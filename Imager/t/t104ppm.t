use Imager ':all';

print "1..3\n";

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


# FIXME: may need tests for 1,2,4 channel images?
