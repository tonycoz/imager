#!perl -w
use Imager ':all';
use lib 't';
use Test::More tests => 64;
use strict;

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
ok(i_writeppm_wiol($img, $IO), "write pnm low")
  or die "Cannot write testout/t104.ppm\n";
close($fh);

$IO = Imager::io_new_bufchain();
ok(i_writeppm_wiol($img, $IO), "write to bufchain")
  or die "Cannot write to bufchain";
my $data = Imager::io_slurp($IO);

$fh = openimage("testout/t104.ppm");
$IO = Imager::io_new_fd( fileno($fh) );
my $cmpimg = i_readpnm_wiol($IO,-1);
ok($cmpimg, "read image we wrote")
  or die "Cannot read testout/t104.ppm\n";
close($fh);

is(i_img_diff($img, $cmpimg), 0, "compare written and read images");

my $rdata = slurp("testout/t104.ppm");
is($data, $rdata, "check data read from file and bufchain data");

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
ok(i_writeppm_wiol($gimg, $IO), "write grayscale");
close FH;

open FH, "< testout/t104_gray.pgm" or die "Cannot open testout/t104_gray.pgm: $!\n";
binmode FH;
$IO = Imager::io_new_fd(fileno(FH));
my $gcmpimg = i_readpnm_wiol($IO, -1);
ok($gcmpimg, "read grayscale");
is(i_img_diff($gimg, $gcmpimg), 0, 
   "compare written and read greyscale images");

my $ooim = Imager->new;
ok($ooim->read(file=>"testimg/simple.pbm"), "read simple pbm, via OO");

check_gray(Imager::i_get_pixel($ooim->{IMG}, 0, 0), 255);
check_gray(Imager::i_get_pixel($ooim->{IMG}, 0, 1), 0);
check_gray(Imager::i_get_pixel($ooim->{IMG}, 1, 0), 0);
check_gray(Imager::i_get_pixel($ooim->{IMG}, 1, 1), 255);

{
  # https://rt.cpan.org/Ticket/Display.html?id=7465
  # the pnm reader ignores the maxval that it reads from the pnm file
  my $maxval = Imager->new;
  ok($maxval->read(file=>"testimg/maxval.ppm"),
     "read testimg/maxval.ppm");
  
  # this image contains three pixels, with each sample from 0 to 63
  # the pixels are (63, 63, 63), (32, 32, 32) and (31, 31, 0)
  
  # check basic parameters
  is($maxval->getchannels, 3, "channel count");
  is($maxval->getwidth, 3, "width");
  is($maxval->getheight, 1, "height");
  
  # check the pixels
  ok(my ($white, $grey, $green) = $maxval->getpixel('x'=>[0,1,2], 'y'=>[0,0,0]), "fetch pixels");
  check_color($white, 255, 255, 255, "white pixel");
  check_color($grey,  130, 130, 130, "grey  pixel");
  check_color($green, 125, 125, 0,   "green pixel");

  # and do the same for ASCII images
  my $maxval_asc = Imager->new;
  ok($maxval_asc->read(file=>"testimg/maxval_asc.ppm"),
     "read testimg/maxval_asc.ppm");
  
  # this image contains three pixels, with each sample from 0 to 63
  # the pixels are (63, 63, 63), (32, 32, 32) and (31, 31, 0)
  
  # check basic parameters
  is($maxval_asc->getchannels, 3, "channel count");
  is($maxval_asc->getwidth, 3, "width");
  is($maxval_asc->getheight, 1, "height");
  
  # check the pixels
  ok(my ($white_asc, $grey_asc, $green_asc) = $maxval_asc->getpixel('x'=>[0,1,2], 'y'=>[0,0,0]), "fetch pixels");
  check_color($white_asc, 255, 255, 255, "white asc pixel");
  check_color($grey_asc,  130, 130, 130, "grey  asc pixel");
  check_color($green_asc, 125, 125, 0,   "green asc pixel");
}

{ # previously we didn't validate maxval at all, make sure it's
  # validated now
  my $maxval0 = Imager->new;
  ok(!$maxval0->read(file=>'testimg/maxval_0.ppm'),
     "should fail to read maxval 0 image");
  print "# ", $maxval0->errstr, "\n";
  like($maxval0->errstr, qr/maxval is zero - invalid pnm file/,
       "error expected from reading maxval_0.ppm");

  my $maxval65536 = Imager->new;
  ok(!$maxval65536->read(file=>'testimg/maxval_65536.ppm'),
     "should fail reading maxval 65536 image");
  print "# ",$maxval65536->errstr, "\n";
  like($maxval65536->errstr, qr/maxval of 65536 is over 65535 - invalid pnm file/,
       "error expected from reading maxval_65536.ppm");

  # maxval of 256 is valid, but Imager can't handle it yet in binary files
  my $maxval256 = Imager->new;
  ok(!$maxval256->read(file=>'testimg/maxval_256.ppm'),
     "should fail reading maxval 256 image");
  print "# ",$maxval256->errstr,"\n";
  like($maxval256->errstr, qr/maxval of 256 is over 255 - not currently supported by Imager/,
       "error expected from reading maxval_256.ppm");

  # make sure we handle maxval > 255 for ascii
  my $maxval4095asc = Imager->new;
  ok($maxval4095asc->read(file=>'testimg/maxval_4095_asc.ppm'),
     "read maxval_4095_asc.ppm");
  is($maxval4095asc->getchannels, 3, "channels");
  is($maxval4095asc->getwidth, 3, "width");
  is($maxval4095asc->getheight, 1, "height");

  ok(my ($white, $grey, $green) = $maxval4095asc->getpixel('x'=>[0,1,2], 'y'=>[0,0,0]), "fetch pixels");
  check_color($white, 255, 255, 255, "white 4095 pixel");
  check_color($grey,  128, 128, 128, "grey  4095 pixel");
  check_color($green, 127, 127, 0,   "green 4095 pixel");
}

{ # check i_format is set when reading a pnm file
  # doesn't really matter which file.
  my $maxval = Imager->new;
  ok($maxval->read(file=>"testimg/maxval.ppm"),
      "read test file");
  my ($type) = $maxval->tags(name=>'i_format');
  is($type, 'pnm', "check i_format");
}

{ # check file limits are checked
  my $limit_file = "testout/t104.ppm";
  ok(Imager->set_file_limits(reset=>1, width=>149), "set width limit 149");
  my $im = Imager->new;
  ok(!$im->read(file=>$limit_file),
     "should fail read due to size limits");
  print "# ",$im->errstr,"\n";
  like($im->errstr, qr/image width/, "check message");

  ok(Imager->set_file_limits(reset=>1, height=>149), "set height limit 149");
  ok(!$im->read(file=>$limit_file),
     "should fail read due to size limits");
  print "# ",$im->errstr,"\n";
  like($im->errstr, qr/image height/, "check message");

  ok(Imager->set_file_limits(reset=>1, width=>150), "set width limit 150");
  ok($im->read(file=>$limit_file),
     "should succeed - just inside width limit");
  ok(Imager->set_file_limits(reset=>1, height=>150), "set height limit 150");
  ok($im->read(file=>$limit_file),
     "should succeed - just inside height limit");
  
  # 150 x 150 x 3 channel image uses 67500 bytes
  ok(Imager->set_file_limits(reset=>1, bytes=>67499),
     "set bytes limit 67499");
  ok(!$im->read(file=>$limit_file),
     "should fail - too many bytes");
  print "# ",$im->errstr,"\n";
  like($im->errstr, qr/storage size/, "check error message");
  ok(Imager->set_file_limits(reset=>1, bytes=>67500),
     "set bytes limit 67500");
  ok($im->read(file=>$limit_file),
     "should succeed - just inside bytes limit");
  Imager->set_file_limits(reset=>1);
}

{ # check error messages set correctly
  my $im = Imager->new(xsize=>100, ysize=>100, channels=>4);
  ok(!$im->write(file=>"testout/t104_fail.ppm", type=>'pnm'),
     "should fail to write 4 channel image");
  is($im->errstr, 'can only save 1 or 3 channel images to pnm',
     "check error message");
  ok(!$im->read(file=>'t/t104ppm.t', type=>'pnm'),
     'should fail to read script as an image file');
  is($im->errstr, 'unable to read pnm image: bad header magic, not a PNM file',
     "check error message");
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
  my ($c, $gray) = @_;

  my ($g) = $c->rgba;
  is($g, $gray, "compare gray");
}

sub check_color {
  my ($c, $red, $green, $blue, $note) = @_;

  my ($r, $g, $b) = $c->rgba;
  is_deeply([ $r, $g, $b], [ $red, $green, $blue ],
	    "$note ($r, $g, $b) compared to ($red, $green, $blue)");
}
