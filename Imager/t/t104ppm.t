#!perl -w
use Imager ':all';
BEGIN { require "t/testtools.pl"; }
use strict;

print "1..45\n";

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
my $IO = Imager::io_new_fd(fileno($fh));
i_writeppm_wiol($img, $IO) 
  or die "Cannot write testout/t104.ppm\n";
close($fh);

print "ok 1\n";

$IO = Imager::io_new_bufchain();
i_writeppm_wiol($img, $IO) or die "Cannot write to bufchain";
my $data = Imager::io_slurp($IO);
print "ok 2\n";

$fh = openimage("testout/t104.ppm");
$IO = Imager::io_new_fd( fileno($fh) );
my $cmpimg = i_readpnm_wiol($IO,-1) || die "Cannot read testout/t104.ppm\n";
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
$IO = Imager::io_new_fd(fileno(FH));
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

my $ooim = Imager->new;
$ooim->read(file=>"testimg/simple.pbm") or print "not ";
print "ok 9\n";

check_gray(10, Imager::i_get_pixel($ooim->{IMG}, 0, 0), 255);
check_gray(11, Imager::i_get_pixel($ooim->{IMG}, 0, 1), 0);
check_gray(12, Imager::i_get_pixel($ooim->{IMG}, 1, 0), 0);
check_gray(13, Imager::i_get_pixel($ooim->{IMG}, 1, 1), 255);

{
  # https://rt.cpan.org/Ticket/Display.html?id=7465
  # the pnm reader ignores the maxval that it reads from the pnm file
  my $maxval = Imager->new;
  $maxval->read(file=>"testimg/maxval.ppm") or print "not ";
  print "ok 14 # read testimg/maxval.ppm\n";
  
  # this image contains three pixels, with each sample from 0 to 63
  # the pixels are (63, 63, 63), (32, 32, 32) and (31, 31, 0)
  
  # check basic parameters
  $maxval->getchannels == 3 or print "not ";
  print "ok 15 # channel count\n";
  $maxval->getwidth == 3 or print "not ";
  print "ok 16 # width\n";
  $maxval->getheight == 1 or print "not ";
  print "ok 17 # height\n";
  
  # check the pixels
  my ($white, $grey, $green) = $maxval->getpixel('x'=>[0,1,2], 'y'=>[0,0,0])
    or print "not ";
  print "ok 18 # fetch pixels\n";
  check_color(19, $white, 255, 255, 255, "white pixel");
  check_color(20, $grey,  130, 130, 130, "grey  pixel");
  check_color(21, $green, 125, 125, 0,   "green pixel");

  # and do the same for ASCII images
  my $maxval_asc = Imager->new;
  $maxval_asc->read(file=>"testimg/maxval_asc.ppm") or print "not ";
  print "ok 22 # read testimg/maxval_asc.ppm\n";
  
  # this image contains three pixels, with each sample from 0 to 63
  # the pixels are (63, 63, 63), (32, 32, 32) and (31, 31, 0)
  
  # check basic parameters
  $maxval_asc->getchannels == 3 or print "not ";
  print "ok 23 # channel count\n";
  $maxval_asc->getwidth == 3 or print "not ";
  print "ok 24 # width\n";
  $maxval_asc->getheight == 1 or print "not ";
  print "ok 25 # height\n";
  
  # check the pixels
  my ($white_asc, $grey_asc, $green_asc) = $maxval_asc->getpixel('x'=>[0,1,2], 'y'=>[0,0,0])
    or print "not ";
  print "ok 26 # fetch pixels\n";
  check_color(27, $white_asc, 255, 255, 255, "white asc pixel");
  check_color(28, $grey_asc,  130, 130, 130, "grey  asc pixel");
  check_color(29, $green_asc, 125, 125, 0,   "green asc pixel");
}

{ # previously we didn't validate maxval at all, make sure it's
  # validated now
  my $maxval0 = Imager->new;
  $maxval0->read(file=>'testimg/maxval_0.ppm') and print "not ";
  print "ok 30 # reading maxval 0 image\n";
  print "# ", $maxval0->errstr, "\n";
  $maxval0->errstr =~ /maxval is zero - invalid pnm file/
    or print "not ";
  print "ok 31 # error expected from reading maxval_0.ppm\n";

  my $maxval65536 = Imager->new;
  $maxval65536->read(file=>'testimg/maxval_65536.ppm') and print "not ";
  print "ok 32 # reading maxval 65536 image\n";
  print "# ",$maxval65536->errstr, "\n";
  $maxval65536->errstr =~ /maxval of 65536 is over 65535 - invalid pnm file/
    or print "not ";
  print "ok 33 # error expected from reading maxval_65536.ppm\n";

  # maxval of 256 is valid, but Imager can't handle it yet in binary files
  my $maxval256 = Imager->new;
  $maxval256->read(file=>'testimg/maxval_256.ppm') and print "not ";
  print "ok 34 # reading maxval 256 image\n";
  print "# ",$maxval256->errstr,"\n";
  $maxval256->errstr =~ /maxval of 256 is over 255 - not currently supported by Imager/
    or print "not ";
  print "ok 35 # error expected from reading maxval_256.ppm\n";

  # make sure we handle maxval > 255 for ascii
  my $maxval4095asc = Imager->new;
  okn(36, $maxval4095asc->read(file=>'testimg/maxval_4095_asc.ppm'),
     "read maxval_4095_asc.ppm");
  okn(37, $maxval4095asc->getchannels == 3, "channels");
  okn(38, $maxval4095asc->getwidth == 3, "width");
  okn(39, $maxval4095asc->getheight == 1, "height");

  my ($white, $grey, $green) = $maxval4095asc->getpixel('x'=>[0,1,2], 'y'=>[0,0,0])
    or print "not ";
  print "ok 40 # fetch pixels\n";
  check_color(41, $white, 255, 255, 255, "white 4095 pixel");
  check_color(42, $grey,  128, 128, 128, "grey  4095 pixel");
  check_color(43, $green, 127, 127, 0,   "green 4095 pixel");
}

my $num = 44;
{ # check i_format is set when reading a pnm file
  # doesn't really matter which file.
  my $maxval = Imager->new;
  okn($num++, $maxval->read(file=>"testimg/maxval.ppm"),
      "read test file");
  my ($type) = $maxval->tags(name=>'i_format');
  isn($num++, $type, 'pnm', "check i_format");
}

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

sub check_gray {
  my ($num, $c, $gray) = @_;

  my ($g) = $c->rgba;
  if ($g == $gray) {
    print "ok $num\n";
  }
  else {
    print "not ok $num # $g doesn't match $gray\n";
  }
}

sub check_color {
  my ($num, $c, $red, $green, $blue, $note) = @_;

  my ($r, $g, $b) = $c->rgba;
  if ($r == $red && $g == $green && $b == $blue) {
    print "ok $num # $note\n";
  }
  else {
    print "not ok $num # ($r, $g, $b) doesn't match ($red, $green, $blue)\n";
  }
}
