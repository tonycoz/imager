use Imager ':all';

print "1..8\n";

init_log("testout/t104ppm.log",1);

my $green = i_color_new(0,255,0,255);
my $blue  = i_color_new(0,0,255,255);
my $red   = i_color_new(255,0,0,255);

my $img    = Imager::ImgRaw::new(150,150,3);

i_box_filled($img,70,25,130,125,$green);
i_box_filled($img,20,25,80,125,$blue);
i_arc($img,75,75,30,0,361,$red);
i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

my $fh = openimage(">testout/t104.ppm");
$IO = Imager::io_new_fd(fileno($fh));
i_writeppm_wiol($img, $IO) 
  or die "Cannot write testout/t104.ppm\n";
close($fh);

print "ok 1\n";

$IO = Imager::io_new_bufchain();
i_writeppm_wiol($img, $IO) or die "Cannot write to bufchain";
$data = Imager::io_slurp($IO);
print "ok 2\n";

$fh = openimage("testout/t104.ppm");
$IO = Imager::io_new_fd( fileno($fh) );
$cmpimg = i_readpnm_wiol($IO,-1) || die "Cannot read testout/t104.ppm\n";
close($fh);
print "ok 3\n";

print i_img_diff($img, $cmpimg) ? "not ok 4 # saved image different\n" : "ok 4\n";

my $rdata = slurp("testout/t104.ppm");
print "not " if $rdata ne $data;
print "ok 5\n";


# build a grayscale image
my $gimg = Imager::ImgRaw::new(150, 150, 1);
my $gray = i_color_new(128, 0, 0, 255);
my $dgray = i_color_new(64, 0, 0, 255);
my $white = i_color_new(255, 0, 0, 255);
i_box_filled($gimg, 20, 20, 130, 130, $gray);
i_box_filled($gimg, 40, 40, 110, 110, $dgray);
i_arc($gimg, 75, 75, 30, 0, 361, $white);

open FH, "> testout/t104_gray.pgm" or die "Cannot create testout/t104_gray.pgm: $!\n";
binmode FH;
my $IO = Imager::io_new_fd(fileno(FH));
i_writeppm_wiol($gimg, $IO) or print "not ";
print "ok 6\n";
close FH;

open FH, "< testout/t104_gray.pgm" or die "Cannot open testout/t104_gray.pgm: $!\n";
binmode FH;
$IO = Imager::io_new_fd(fileno(FH));
my $gcmpimg = i_readpnm_wiol($IO, -1) or print "not ";
print "ok 7\n";
i_img_diff($gimg, $gcmpimg) == 0 or print "not ";
print "ok 8\n";

sub openimage {
  my $fname = shift;
  local(*FH);
  open(FH, $fname) or die "Cannot open $fname: $!\n";
  binmode(FH);
  return *FH;
}

sub slurp {
  my $fh = openimage(shift);
  local $/;
  my $data = <$fh>;
  close($fh);
  return $data;
}
