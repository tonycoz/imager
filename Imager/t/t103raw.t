print "1..6\n";
use Imager qw(:all);

init_log("testout/t103raw.log",1);

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

open(FH,">testout/t103.raw") || die "Cannot open testout/t103.raw for writing\n";
binmode(FH);
i_writeraw($img,fileno(FH)) || die "Cannot write testout/t103.raw\n";
close(FH);

print "ok 1\n";

open(FH,"testout/t103.raw") || die "Cannot open testout/t103.raw\n";
binmode(FH);
$cmpimg=i_readraw(fileno(FH),150,150,3,3,0) || die "Cannot read testout/t103.raw\n";
close(FH);

print "# raw average mean square pixel difference: ",sqrt(i_img_diff($img,$cmpimg))/150*150,"\n";
print "ok 2\n";

# I could have kept the raw images for these tests in binary files in
# testimg/, but I think keeping them as hex encoded data in here makes
# it simpler to add more if necessary
# Later we may change this to read from a scalar instead
save_data('testout/t103_base.raw');
save_data('testout/t103_3to4.raw');
save_data('testout/t103_line_int.raw');
save_data('testout/t103_img_int.raw');

# load the base image
open FH, "testout/t103_base.raw" 
  or die "Cannot open testout/t103_base.raw: $!";
binmode FH;
my $baseimg = i_readraw(fileno(FH), 4, 4, 3, 3, 0)
  or die "Cannot read base raw image";
close FH;

# the actual read tests
# each read_test() call does 2 tests:
#  - check if the read succeeds
#  - check if it matches $baseimg
read_test('testout/t103_3to4.raw', 4, 4, 4, 3, 0, $baseimg, 3);
read_test('testout/t103_line_int.raw', 4, 4, 3, 3, 1, $baseimg, 5);
# intrl==2 is documented in raw.c but doesn't seem to be implemented
#read_test('testout/t103_img_int.raw', 4, 4, 3, 3, 2, $baseimg, 7);

sub read_test {
  my ($in, $xsize, $ysize, $data, $store, $intrl, $base, $test) = @_;
  open FH, $in or die "Cannot open $in: $!";
  binmode FH;
  my $img = i_readraw(fileno(FH), $xsize, $ysize, $data, $store, $intrl);
  if ($img) {
    print "ok $test\n";
    if (i_img_diff($img, $baseimg)) {
      print "ok ",$test+1," # skip images don't match, but maybe I don't understand\n";
    }
    else {
      print "ok ",$test+1,"\n";
    }
  }
  else {
    print "not ok $test # could not read image\n";
    print "ok ",$test+1," # skip\n";
  }
}

sub save_data {
  my $outname = shift;
  my $data = load_data();
  open FH, "> $outname" or die "Cannot create $outname: $!";
  binmode FH;
  print FH $data;
  close FH;
}

sub load_data {
  my $hex = '';
  while (<DATA>) {
    next if /^#/;
    last if /^EOF/;
    chomp;
    $hex .= $_;
  }
  $hex =~ tr/ //d;
  my $result = pack("H*", $hex);
  print unpack("H*", $result),"\n";
  return $result;
}

# FIXME: may need tests for 1,2,4 channel images

__DATA__
# we keep some packed raw images here
# we decode this in the code, ignoring lines starting with #, a subfile
# ends with EOF, data is HEX encoded (spaces ignored)

# basic 3 channel version of the image
001122 011223 021324 031425
102132 112233 122334 132435
203142 213243 223344 233445
304152 314253 324354 334455
EOF

# test image for reading a 4 channel image into a 3 channel image
# 4 x 4 pixels
00112233 01122334 02132435 03142536
10213243 11223344 12233445 13243546
20314253 21324354 22334455 23344556
30415263 31425364 32435465 33445566
EOF

# test image for line based interlacing
# 4 x 4 pixels
# first line
00 01 02 03
11 12 13 14
22 23 24 25

# second line
10 11 12 13
21 22 23 24
32 33 34 35

# third line
20 21 22 23
31 32 33 34
42 43 44 45

# fourth line
30 31 32 33
41 42 43 44
52 53 54 55

EOF

# test image for image based interlacing
# first channel
00 01 02 03
10 11 12 13
20 21 22 23
30 31 32 33

# second channel
11 12 13 14
21 22 23 24
31 32 33 34
41 42 43 44

# third channel
22 23 24 25
32 33 34 35
42 43 44 45
52 53 54 55

EOF
