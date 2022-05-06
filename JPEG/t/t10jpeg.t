#!perl -w
use strict;
use Imager qw(:all);
use Test::More;
use Imager::Test qw(is_color_close3 test_image_raw test_image is_image);

-d "testout" or mkdir "testout";

init_log("testout/t101jpeg.log",1);

$Imager::formats{"jpeg"}
  or plan skip_all => "no jpeg support";

diag "libjpeg version: ", Imager::File::JPEG->libjpeg_version(), "\n";

my $green=i_color_new(0,255,0,255);
my $blue=i_color_new(0,0,255,255);
my $red=i_color_new(255,0,0,255);

my $img=test_image_raw();
my $cmpimg=Imager::ImgRaw::new(150,150,3);

open(FH,">testout/t101.jpg")
  || die "cannot open testout/t101.jpg for writing\n";
binmode(FH);
my $IO = Imager::io_new_fd(fileno(FH));
ok(Imager::File::JPEG::i_writejpeg_wiol($img,$IO,30), "write jpeg low level");
close(FH);

open(FH, "testout/t101.jpg") || die "cannot open testout/t101.jpg\n";
binmode(FH);
$IO = Imager::io_new_fd(fileno(FH));
($cmpimg,undef) = Imager::File::JPEG::i_readjpeg_wiol($IO);
close(FH);

my $diff = sqrt(i_img_diff($img,$cmpimg))/150*150;
print "# jpeg average mean square pixel difference: ",$diff,"\n";
ok($cmpimg, "read jpeg low level");

ok($diff < 10000, "difference between original and jpeg within bounds");

Imager::i_log_entry("Starting 4\n", 1);
my $imoo = Imager->new;
ok($imoo->read(file=>'testout/t101.jpg'), "read jpeg OO");

ok($imoo->write(file=>'testout/t101_oo.jpg'), "write jpeg OO");
Imager::i_log_entry("Starting 5\n", 1);
my $oocmp = Imager->new;
ok($oocmp->read(file=>'testout/t101_oo.jpg'), "read jpeg OO for comparison");

$diff = sqrt(i_img_diff($imoo->{IMG},$oocmp->{IMG}))/150*150;
print "# OO image difference $diff\n";
ok($diff < 10000, "difference between original and jpeg within bounds");

# write failure test
open FH, "< testout/t101.jpg" or die "Cannot open testout/t101.jpg: $!";
binmode FH;
my $io = Imager::io_new_fd(fileno(FH));
$io->set_buffered(0);
ok(!$imoo->write(io => $io, type=>'jpeg'), 'failure handling');
close FH;
print "# ",$imoo->errstr,"\n";

# check that the i_format tag is set
my @fmt = $imoo->tags(name=>'i_format');
is($fmt[0], 'jpeg', 'i_format tag');

{ # check file limits are checked
  my $limit_file = "testout/t101.jpg";
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

SKIP:
{
  # we don't test them all
  my %expected_tags =
    (
     exif_date_time_original => "2005:11:25 00:00:00",
     exif_flash => 0,
     exif_image_description => "Imager Development Notes",
     exif_make => "Canon",
     exif_model => "CanoScan LiDE 35",
     exif_resolution_unit => 2,
     exif_resolution_unit_name => "inches",
     exif_user_comment => "        Part of notes from reworking i_arc() and friends.",
     exif_white_balance => 0,
     exif_white_balance_name => "Auto white balance",
    );
  
  my $im = Imager->new;
  $im->read(file=>"testimg/exiftest.jpg")
    or skip("Could not read test image:".$im->errstr, scalar keys %expected_tags);
  
  for my $key (keys %expected_tags) {
    is($expected_tags{$key}, $im->tags(name => $key),
       "test value of exif tag $key");
  }
}

{
  # tests that the density values are set and read correctly
  # tests jpeg_comment too
  my @density_tests =
    (
     [ 't101cm100.jpg', 
       { 
	jpeg_density_unit => 2, 
	i_xres => 254, 
	i_yres => 254
       },
       { 
	jpeg_density_unit => 2, 
	i_xres => 254, 
	i_yres => 254,
	i_aspect_only => undef,
       },
     ],
     [
      't101xonly.jpg',
      {
       i_xres => 100,
      },
      {
       i_xres => 100,
       i_yres => 100,
       jpeg_density_unit => 1,
       i_aspect_only => undef,
      },
     ],
     [
      't101yonly.jpg',
      {
       i_yres => 100,
      },
      {
       i_xres => 100,
       i_yres => 100,
       jpeg_density_unit => 1,
       i_aspect_only => undef,
      },
     ],
     [
      't101asponly.jpg',
      {
       i_xres => 50,
       i_yres => 100,
       i_aspect_only => 1,
      },
      {
       i_xres => 50,
       i_yres => 100,
       i_aspect_only => 1,
       jpeg_density_unit => 0,
      },
     ],
     [
      't101com.jpg',
      {
       jpeg_comment => 'test comment'
      },
     ],
    );
  
  print "# test density tags\n";
  # I don't care about the content
  my $base_im = Imager->new(xsize => 10, ysize => 10);
  for my $test (@density_tests) {
    my ($filename, $out_tags, $expect_tags) = @$test;
    $expect_tags ||= $out_tags;
    
    my $work = $base_im->copy;
    for my $key (keys %$out_tags) {
      $work->addtag(name => $key, value => $out_tags->{$key});
    }
    
    ok($work->write(file=>"testout/$filename", type=>'jpeg'),
       "save $filename");
    
    my $check = Imager->new;
    ok($check->read(file=> "testout/$filename"),
       "read $filename");
    
    my %tags;
    for my $key (keys %$expect_tags) {
      $tags{$key} = $check->tags(name=>$key);
    }
    is_deeply($expect_tags, \%tags, "check tags for $filename");
  }
}

{ # Issue # 17981
  # the test image has a zero-length user_comment field
  # the code would originally attempt to convert '\0' to ' '
  # for the first 8 bytes, even if the string was less than 
  # 8 bytes long
  my $im = Imager->new;
  ok($im->read(file => 'testimg/209_yonge.jpg', type=>'jpeg'),
     "test read of image with invalid exif_user_comment");
  is($im->tags(name=>'exif_user_comment'), '',
     "check exif_user_comment set correctly");
}

{ # test parseiptc handling no IPTC data correctly
  my $saw_warn;
  local $SIG{__WARN__} = 
    sub {
      ++$saw_warn;
      print "# @_\n";
    };
  my $im = Imager->new;
  ok($im->read(file => 'testout/t101.jpg', type=>'jpeg'),
     "read jpeg with no IPTC data");
  ok(!defined $im->{IPTCRAW}, "no iptc data");
  my %iptc = $im->parseiptc;
  ok(!$saw_warn, "should be no warnings");
}

{ # Issue # 18397
  # attempting to write a 4 channel image to a bufchain would
  # cause a seg fault.
  # it should fail still
  # overridden by # 29876
  # give 4/2 channel images a background color when saving to JPEG
  my $im = Imager->new(xsize => 16, ysize => 16, channels => 4);
  $im->box(filled => 1, xmin => 8, color => '#FFE0C0');
  my $data;
  ok($im->write(data => \$data, type => 'jpeg'),
     "should write with a black background");
  my $imread = Imager->new;
  ok($imread->read(data => $data, type => 'jpeg'), 'read it back');
  is_color_close3($imread->getpixel('x' => 0, 'y' => 0), 0, 0, 0, 4,
		  "check it's black");
  is_color_close3($imread->getpixel('x' => 15, 'y' => 9), 255, 224, 192, 4,
		  "check filled area filled");
  
  # write with a red background
  $data = '';
  ok($im->write(data => \$data, type => 'jpeg', i_background => '#FF0000'),
     "write with red background");
  ok($imread->read(data => $data, type => 'jpeg'), "read it back");
  is_color_close3($imread->getpixel('x' => 0, 'y' => 0), 255, 0, 0, 4,
		  "check it's red");
  is_color_close3($imread->getpixel('x' => 15, 'y' => 9), 255, 224, 192, 4,
		  "check filled area filled");
}
SKIP:
{ # Issue # 18496
  # If a jpeg with EXIF data containing an (invalid) IFD entry with a 
  # type of zero is read then Imager crashes with a Floating point 
  # exception
  # testimg/zerojpeg.jpg was manually modified from exiftest.jpg to
  # reproduce the problem.
  my $im = Imager->new;
  ok($im->read(file=>'testimg/zerotype.jpg'), "shouldn't crash");
}

SKIP:
{ # code coverage - make sure wiol_skip_input_data is called
  open BASEDATA, "< testimg/exiftest.jpg"
    or skip "can't open base data", 1;
  binmode BASEDATA;
  my $data = do { local $/; <BASEDATA> };
  close BASEDATA;
  
  substr($data, 3, 1) eq "\xE1"
    or skip "base data isn't as expected", 1;
  # inserting a lot of marker data here means we take the branch in 
  # wiol_skip_input_data that refills the buffer
  my $marker = "\xFF\xE9"; # APP9 marker
  $marker .= pack("n", 8192) . "x" x 8190;
  $marker x= 10; # make it take up a lot of space
  substr($data, 2, 0) = $marker;
  my $im = Imager->new;
  ok($im->read(data => $data), "read with a skip of data");
}

SKIP:
{ # code coverage - take the branch that provides a fake EOI
  open BASEDATA, "< testimg/exiftest.jpg"
    or skip "can't open base data", 1;
  binmode BASEDATA;
  my $data = do { local $/; <BASEDATA> };
  close BASEDATA;
  substr($data, -1000) = '';
  
  my $im = Imager->new;
  ok($im->read(data => $data), "read with image data truncated");
}

{ # code coverage - make sure wiol_empty_output_buffer is called
  my $im = Imager->new(xsize => 1000, ysize => 1000);
  for my $x (0 .. 999) {
    $im->line(x1 => $x, y1 => 0, x2 => $x, y2 => 999,
	      color => Imager::Color->new(rand 256, rand 256, rand 256));
  }
  my $data;
  ok($im->write(data => \$data, type=>'jpeg', jpegquality => 100), 
     "write big file to ensure wiol_empty_output_buffer is called")
    or print "# ", $im->errstr, "\n";
  
  # code coverage - write failure path in wiol_empty_output_buffer
  ok(!$im->write(callback => sub { return },
		 type => 'jpeg', jpegquality => 100),
     "fail to write")
    and print "# ", $im->errstr, "\n";
}

{ # code coverage - virtual image branch in i_writejpeg_wiol()
  my $im = $imoo->copy;
  my $immask = $im->masked;
  ok($immask, "made a virtual image (via masked)");
  ok($immask->virtual, "check it's virtual");
  my $mask_data;
  ok($immask->write(data => \$mask_data, type => 'jpeg'),
     "write masked version");
  my $base_data;
  ok($im->write(data => \$base_data, type=>'jpeg'),
     "write normal version");
  is($base_data, $mask_data, "check the data written matches");
}

SKIP:
{ # code coverage - IPTC data
  # this is dummy data
  my $iptc = "\x04\x04" .
    "\034\002x   My Caption"
      . "\034\002P   Tony Cook"
	. "\034\002i   Dummy Headline!"
	  . "\034\002n   No Credit Given";
  
  my $app13 = "\xFF\xED" . pack("n", 2 + length $iptc) . $iptc;
  
  open BASEDATA, "< testimg/exiftest.jpg"
    or skip "can't open base data", 1;
  binmode BASEDATA;
  my $data = do { local $/; <BASEDATA> };
  close BASEDATA;
  substr($data, 2, 0) = $app13;
  
  my $im = Imager->new;
  ok($im->read(data => $data), "read with app13 data");
  my %iptc = $im->parseiptc;
  is($iptc{caption}, 'My Caption', 'check iptc caption');
  is($iptc{photogr}, 'Tony Cook', 'check iptc photogr');
  is($iptc{headln}, 'Dummy Headline!', 'check iptc headln');
  is($iptc{credit}, 'No Credit Given', 'check iptc credit');
}

{ # handling of CMYK jpeg
  # http://rt.cpan.org/Ticket/Display.html?id=20416
  my $im = Imager->new;
  ok($im->read(file => 'testimg/scmyk.jpg'), 'read a CMYK jpeg');
  is($im->getchannels, 3, "check channel count");
  my $col = $im->getpixel(x => 0, 'y' => 0);
  ok($col, "got the 'black' pixel");
  # this is jpeg, so we can't compare colors exactly
  # older versions returned this pixel at a light color, but
  # it's black in the image
  my ($r, $g, $b) = $col->rgba;
  cmp_ok($r, '<', 10, 'black - red low');
  cmp_ok($g, '<', 10, 'black - green low');
  cmp_ok($b, '<', 10, 'black - blue low');
  $col = $im->getpixel(x => 15, 'y' => 0);
  ok($col, "got the dark blue");
  ($r, $g, $b) = $col->rgba;
  cmp_ok($r, '<', 10, 'dark blue - red low');
  cmp_ok($g, '<', 10, 'dark blue - green low');
  cmp_ok($b, '>', 110, 'dark blue - blue middle (bottom)');
  cmp_ok($b, '<', 130, 'dark blue - blue middle (top)');
  $col = $im->getpixel(x => 0, 'y' => 15);
  ok($col, "got the red");
  ($r, $g, $b) = $col->rgba;
  cmp_ok($r, '>', 245, 'red - red high');
  cmp_ok($g, '<', 10, 'red - green low');
  cmp_ok($b, '<', 10, 'red - blue low');
}

{
  ok(grep($_ eq 'jpeg', Imager->read_types), "check jpeg in read types");
  ok(grep($_ eq 'jpeg', Imager->write_types), "check jpeg in write types");
}

{ # progressive JPEG
  # https://rt.cpan.org/Ticket/Display.html?id=68691
  my $im = test_image();
  my $progim = $im->copy;

  ok($progim->write(file => "testout/t10prog.jpg", type => "jpeg",
		    jpeg_progressive => 1),
     "write progressive jpeg");

  my $rdprog = Imager->new(file => "testout/t10prog.jpg");
  ok($rdprog, "read progressive jpeg");
  my @prog = $rdprog->tags(name => "jpeg_progressive");
  is($prog[0], 1, "check progressive flag set on read");

  my $data;
  ok($im->write(data => \$data, type => "jpeg"), 
     "save as non-progressive to compare");
  my $norm = Imager->new(data => $data);
  ok($norm, "read non-progressive file");
  my @nonprog = $norm->tags(name => "jpeg_progressive");
  is($nonprog[0], 0, "check progressive flag 0 for non prog file");

  is_image($rdprog, $norm, "prog vs norm should be the same image");
}

SKIP:
{ # optimize coding
  my $im = test_image();
  my $base;
  ok($im->write(data => \$base, type => "jpeg"), "save without optimize");
  my $opt;
  ok($im->write(data => \$opt, type => "jpeg", jpeg_optimize => 1),
     "save with optimize");
  cmp_ok(length $opt, '<', length $base, "check optimized is smaller");
  my $im_base = Imager->new(data => $base, filetype => "jpeg");
  ok($im_base, "read unoptimized back");
  my $im_opt = Imager->new(data => $opt, filetype => "jpeg");
  ok($im_opt, "read optimized back");
  $im_base && $im_opt
    or skip "couldn't read one back", 1;
  is_image($im_opt, $im_base,
	   "optimization should only change huffman compression, not quality");
}

SKIP:
{
  my $im = test_image();
  # unknown jpeg_compress_profile always fails, doesn't matter whether we have mozjpeg
  my $data;
  ok(!$im->write(data => \$data, type => "jpeg", jpeg_compress_profile => "invalid"),
     "fail to write with unknown compression profile");
  like($im->errstr, qr/jpeg_compress_profile=invalid unknown/,
       "check the error message");

  ok($im->write(data => \$data, type => "jpeg", jpeg_compress_profile => "fastest"),
     "jpeg_compress_profile=fastest is always available");
  Imager::File::JPEG->is_mozjpeg
      and skip "this is mozjpeg", 1;
  ok(!$im->write(data => \$data, type => "jpeg", jpeg_compress_profile => "max"),
     "fail to write with max compression profile without mozjpeg");
  like($im->errstr, qr/jpeg_compress_profile=max requires mozjpeg/,
       "check the error message");
}

SKIP:
{
  Imager::File::JPEG->is_mozjpeg
      or skip "this isn't mozjpeg", 1;
  my $im = test_image();
  my $base_data;
  ok($im->write(data => \$base_data, type => "jpeg", jpeg_compress_profile => "fastest"),
     "write using defaults");
  my $opt_data;
  ok($im->write(data => \$opt_data,  type => "jpeg", jpeg_compress_profile => "max"),
     "write using max compression profile");
  cmp_ok(length($opt_data), '<', length($base_data),
         "max compression should be smaller");
}

SKIP:
{
  Imager::File::JPEG->has_arith_coding
      or skip "arithmetic coding not available", 1;
  my $im = test_image;
  my $data;
  ok($im->write(data => \$data, type => "jpeg", jpeg_arithmetic => 1),
     "write with arithmetic coding");
  my $im2 = Imager->new(data => $data);
  ok($im2, "read back arithmetic coded");
  ok($im2->tags(name => "jpeg_read_arithmetic"),
     "and read tag set");
}

{
  # default RGB (YCbCr) image has JFIF header
  my $im = test_image;
  my $data;
  ok($im->write(data => \$data, type => "jpeg"),
     "write default RGB");
  my $im2 = Imager->new;
  ok($im2->read(data => $data), "read it back");
  is($im2->tags(name => "jpeg_read_jfif"), 1, "JFIF detected");
  ok($im->write(data => \$data, type => "jpeg", jpeg_jfif => 0),
     "disable the JFIF header");
  ok($im2->read(data => $data), "read back file without JFIF");
  is($im2->tags(name => "jpeg_read_jfif"), 0, "no JFIF detected");
}

{
  my $im = Imager->new(xsize => 150, ysize => 150);
  $im->box(filled => 1, color => "#F00", box => [ 15, 15, 84, 84 ]);
  $im->box(filled => 1, color => "#0F0", box => [ 30, 30, 69, 69 ]);
  # guesstimate
  my $smoothed = $im->copy->filter(type => 'gaussian', stddev => 1);
  my $data_base;
  ok($im->write(data => \$data_base, type => "jpeg"), "write base image (control)");
  my $data_smoothed;
  ok($im->write(data => \$data_smoothed, type => "jpeg", jpeg_smooth => 50),
     "write smoothed image");
  my $smim = Imager->new;
  ok($smim->read(data => $data_smoothed), "read smoothed back");
  my $diffWvsS = Imager::i_img_diff($smim->{IMG}, $smoothed->{IMG});
  my $diffWvsB = Imager::i_img_diff($smim->{IMG}, $im->{IMG});
  cmp_ok($diffWvsS, '<', $diffWvsB,
	 "written image should be closer to blurred image than base image");

  my $data;
  ok(!$im->write(data => \$data, type => "jpeg", jpeg_smooth => 101),
     "invalid smoothing value");
  like($im->errstr, qr/jpeg_smooth must be an integer from 0 to 100/, "check error message");
}

{
  # jpeg_restart
  my $im = test_image;
  my $data;
  ok($im->write(data => \$data,        type => "jpeg"),
     "write with default restarts");
  my $data_8_rows;
  ok($im->write(data => \$data_8_rows, type => "jpeg", jpeg_restart => 8),
     "write with restart every 8 rows");
  my $data_8_mcus;
  ok($im->write(data => \$data_8_mcus, type => "jpeg", jpeg_restart => "8b"),
     "write with restart every 8 mcus");
  cmp_ok(length($data), '<', length($data_8_rows),
         "no restarts smaller than restart every 8 rows");
  cmp_ok(length($data_8_rows), '<', length($data_8_mcus),
         "restarts every 8 rows smaller than restart every 8 mcus");

  # error handling
  ok(!$im->write(data => \$data, type => "jpeg", jpeg_restart => "8c"),
     "bad units value");
  like($im->errstr, qr/jpeg_restart must be an integer from 0 to 65535 followed by an optional b/,
       "check error message");
  $im->_set_error("");
  ok(!$im->write(data => \$data, type => "jpeg", jpeg_restart => "65536"),
     "out of range value");
  like($im->errstr, qr/jpeg_restart must be an integer from 0 to 65535 followed by an optional b/,
       "check error message");
}

SKIP:
{
  # jpeg_sample
  my $im = test_image;
  my $data_base;
  ok($im->write(data => \$data_base, type => "jpeg"),
     "write default");
  my $data_1x1;
  ok($im->write(data => \$data_1x1,  type => "jpeg", jpeg_sample => "1x1"),
     "write with 1x1")
    or diag $im->errstr;
  cmp_ok(length($data_base), '<', length($data_1x1),
         "1x1 sampled file is larger");
  my $baseim = Imager->new(data => $data_base)
    or skip "couldn't read back base image", 1;
  my $im1x1 = Imager->new(data => $data_1x1)
    or skip "couldn't read back 1x1 image", 1;
  my $diff_base = Imager::i_img_diff($im->{IMG}, $baseim->{IMG});
  my $diff_1x1 = Imager::i_img_diff($im->{IMG}, $im1x1->{IMG});
  cmp_ok($diff_1x1, '<', $diff_base,
         "1x1 sampled image should be closer to source image");

  # varieties
  my $data_deftag;
  ok($im->write(data => \$data_deftag, type => "jpeg", jpeg_sample => "2x2,1x1,1x1"),
     "specify default by tag");
  is(length($data_base), length($data_deftag),
     "default set by tag matches default");

  my $data;
  my $errq = quotemeta "jpeg_sample: must match /^[1-4]x[1-4](,[1-4]x[1-4]){0,9}\$/aai";
  my $errqr = qr/$errq/;
  # failures
  ok(!$im->write(data => \$data, type => "jpeg", jpeg_sample => "5x1"),
     "H out of range");
  like($im->errstr, $errqr, "check error message");
  $im->_set_error("");
  ok(!$im->write(data => \$data, type => "jpeg", jpeg_sample => "1x5"),
     "V out of range");
  like($im->errstr, $errqr, "check error message");
  $im->_set_error("");
  ok(!$im->write(data => \$data, type => "jpeg", jpeg_sample => "1x5."),
     "non-comma/eof after V");
  like($im->errstr, $errqr, "check error message");
  $im->_set_error("");
  ok(!$im->write(data => \$data, type => "jpeg", jpeg_sample => "1y1"),
     "non X between H and V");
  like($im->errstr, $errqr, "check error message");
  $im->_set_error("");
  ok(!$im->write(data => \$data, type => "jpeg", jpeg_sample => "1x1,"),
     "orphan comma at end");
  like($im->errstr, $errqr, "check error message");
  $im->_set_error("");
}

my @bool_tags =
  qw(jpeg_optimize_scans jpeg_trellis_quant jpeg_trellis_quant_dc
     jpeg_tresllis_eob_opt jpeg_use_lambda_weight_tbl jpeg_use_scans_in_trellis
     jpeg_overshoot_deringing);

my @float_tags =
  qw(jpeg_lambda_log_scale1 jpeg_lambda_log_scale2 jpeg_trellis_delta_dc_weight);

my @int_tags =
  qw(jpeg_trellis_freq_split jpeg_trellis_num_loops jpeg_base_quant_tbl_idx
     jpeg_dc_scan_opt_mode);
SKIP:
{
  Imager::File::JPEG->is_mozjpeg
      or skip "These tags are mozjpeg only", 1;

  # boolean tags
  for my $tag (@bool_tags) {
    my $im = test_image();
    my $data;
    ok($im->write(data => \$data, type => "jpeg", $tag => 0),
       "write with boolean tag $tag");
  }
  # float tags
  for my $tag (@float_tags) {
    my $im = test_image();
    my $data;
    ok($im->write(data => \$data, type => "jpeg", $tag => 8.1),
       "write with float tag $tag");
  }
  # int tags
  for my $tag (@int_tags) {
    my $im = test_image();
    my $data;
    ok($im->write(data => \$data, type => "jpeg", $tag => 2),
       "write with int tag $tag");
  }
}

SKIP:
{
  Imager::File::JPEG->is_mozjpeg
      and skip "Testing absence of mozjpeg", 1;

  # none of these tags should be settable
  # boolean tags
  for my $tag (@bool_tags) {
    my $im = test_image();
    my $data;
    ok(!$im->write(data => \$data, type => "jpeg", $tag => 0),
       "fail write with boolean tag $tag");
  }
  # float tags
  for my $tag (@float_tags) {
    my $im = test_image();
    my $data;
    ok(!$im->write(data => \$data, type => "jpeg", $tag => 8.1),
       "fail write with float tag $tag");
  }
  # int tags
  for my $tag (@int_tags) {
    my $im = test_image();
    my $data;
    ok(!$im->write(data => \$data, type => "jpeg", $tag => 2),
       "fail write with int tag $tag");
  }
}

SKIP:
{
  Imager::File::JPEG->is_mozjpeg
      or skip "jpeg_tune needs mozjpeg", 1;
  for my $tune (qw(psnr ssim ms-ssim hvs-psnr)) {
    my $data;
    my $im = test_image;
    ok($im->write(data => \$data, type => "jpeg", jpeg_tune => $tune),
       "jpeg_tune=$tune");
    note "size ".length $data;
  }
  {
    my $data;
    my $im = test_image;
    ok(!$im->write(data => \$data, type => "jpeg", jpeg_tune => "rubbish"),
       "fail jpeg_tune=rubbish");
  }
}

SKIP:
{
  Imager::File::JPEG->is_mozjpeg
      and skip "testing absence of mozjpeg", 1;
  for my $tune (qw(psnr ssim ms-ssim hvs-psnr)) {
    my $data;
    my $im = test_image;
    ok(!$im->write(data => \$data, type => "jpeg", jpeg_tune => $tune),
       "fail jpeg_tune=$tune");
    note "size ".length $data;
  }
}

{ # check close failures are handled correctly
  my $im = test_image();
  my $fail_close = sub {
    Imager::i_push_error(0, "synthetic close failure");
    return 0;
  };
  ok(!$im->write(type => "jpeg", callback => sub { 1 },
		 closecb => $fail_close),
     "check failing close fails");
    like($im->errstr, qr/synthetic close failure/,
	 "check error message");
}

done_testing();
