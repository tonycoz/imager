#!perl -w
print "1..74\n";
use Imager qw(:all);
use strict;
init_log("testout/t107bmp.log",1);
BEGIN { require 't/testtools.pl'; } # BEGIN to apply prototypes

my $base_diff = 0;
# if you change this make sure you generate new compressed versions
my $green=i_color_new(0,255,0,255);
my $blue=i_color_new(0,0,255,255);
my $red=i_color_new(255,0,0,255);

my $img=Imager::ImgRaw::new(150,150,3);

i_box_filled($img,70,25,130,125,$green);
i_box_filled($img,20,25,80,125,$blue);
i_arc($img,75,75,30,0,361,$red);
i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

Imager::i_tags_add($img, 'i_xres', 0, '300', 0);
Imager::i_tags_add($img, 'i_yres', 0, undef, 300);
write_test(1, $img, "testout/t107_24bit.bmp");
# 'webmap' is noticably faster than the default
my $im8 = Imager::i_img_to_pal($img, { make_colors=>'webmap', 
				       translate=>'errdiff'});
write_test(2, $im8, "testout/t107_8bit.bmp");
# use a fixed palette so we get reproducible results for the compressed
# version
my @pal16 = map { NC($_) } 
  qw(605844 966600 0148b2 00f800 bf0a33 5e009e
     2ead1b 0000f8 004b01 fd0000 0e1695 000002);
my $im4 = Imager::i_img_to_pal($img, { colors=>\@pal16, make_colors=>'none' });
write_test(3, $im4, "testout/t107_4bit.bmp");
my $im1 = Imager::i_img_to_pal($img, { colors=>[ NC(0, 0, 0), NC(176, 160, 144) ],
			       make_colors=>'none', translate=>'errdiff' });
write_test(4, $im1, "testout/t107_1bit.bmp");
my $bi_rgb = 0;
my $bi_rle8 = 1;
my $bi_rle4 = 2;
my $bi_bitfields = 3;
read_test(5, "testout/t107_24bit.bmp", $img, 
          bmp_compression=>0, bmp_bit_count => 24);
read_test(6, "testout/t107_8bit.bmp", $im8, 
          bmp_compression=>0, bmp_bit_count => 8);
read_test(7, "testout/t107_4bit.bmp", $im4, 
          bmp_compression=>0, bmp_bit_count => 4);
read_test(8, "testout/t107_1bit.bmp", $im1, bmp_compression=>0, 
          bmp_bit_count=>1);
# the following might have slight differences
$base_diff = i_img_diff($img, $im8) * 2;
print "# base difference $base_diff\n";
read_test(9, "testimg/comp4.bmp", $im4, 
          bmp_compression=>$bi_rle4, bmp_bit_count => 4);
read_test(10, "testimg/comp8.bmp", $im8, 
          bmp_compression=>$bi_rle8, bmp_bit_count => 8);

my $imoo = Imager->new;
if ($imoo->read(file=>'testout/t107_24bit.bmp')) {
  print "ok 11\n";
}
else {
  print "not ok 11 # ",$imoo->errstr,"\n";
}
if ($imoo->write(file=>'testout/t107_oo.bmp')) {
  print "ok 12\n";
}
else {
  print "not 12 # ",$imoo->errstr,"\n";
}

# various invalid format tests
# we have so many different test images to try to detect all the possible
# failure paths in the code, adding these did detect real problems
print "# catch various types of invalid bmp files\n";
my $test_num = 13;
my @tests =
  (
   # entries in each array ref are:
   #  - basename of an invalid BMP file
   #  - error message that should be produced
   #  - description of what is being tested
   #  - possible flag to indicate testing only on 32-bit machines
   [ 'badplanes.bmp', 'not a BMP file', "invalid planes value" ],
   [ 'badbits.bmp', 'unknown bit count for BMP file (5)', 
     'should fail to read invalid bits' ],

   # 1-bit/pixel BMPs
   [ 'badused1.bmp', 'out of range colors used (3)',
     'out of range palette size (1-bit)' ],
   [ 'badcomp1.bmp', 'unknown 1-bit BMP compression (1)',
     'invalid compression value (1-bit)' ],
   [ 'bad1wid0.bmp', 'Image sizes must be positive',
     'width 0 (1-bit)' ],
   [ 'bad4oflow.bmp', 'integer overflow calculating image allocation',
     'overflow integers on 32-bit machines (1-bit)', '32bitonly' ],
   [ 'short1.bmp', 'failed reading 1-bit bmp data', 
     'short 1-bit' ],

   # 4-bit/pixel BMPs
   [ 'badused4a.bmp', 'out of range colors used (272)', 
     'should fail to read invalid pal size (272) (4-bit)' ],
   [ 'badused4b.bmp', 'out of range colors used (17)',
     'should fail to read invalid pal size (17) (4-bit)' ],
   [ 'badcomp4.bmp', 'unknown 4-bit BMP compression (1)',
     'invalid compression value (4-bit)' ],
   [ 'short4.bmp', 'failed reading 4-bit bmp data', 
     'short uncompressed 4-bit' ],
   [ 'short4rle.bmp', 'missing data during decompression', 
     'short compressed 4-bit' ],
   [ 'bad4wid0.bmp', 'Image sizes must be positive',
     'width 0 (4-bit)' ],
   [ 'bad4widbig.bmp', 'Image sizes must be positive',
     'width big (4-bit)' ],
   [ 'bad4oflow.bmp', 'integer overflow calculating image allocation',
     'overflow integers on 32-bit machines (4-bit)', '32bitonly' ],

   # 8-bit/pixel BMPs
   [ 'bad8useda.bmp', 'out of range colors used (257)',
     'should fail to read invalid pal size (8-bit)' ],
   [ 'bad8comp.bmp', 'unknown 8-bit BMP compression (2)',
     'invalid compression value (8-bit)' ],
   [ 'short8.bmp', 'failed reading 8-bit bmp data', 
     'short uncompressed 8-bit' ],
   [ 'short8rle.bmp', 'missing data during decompression', 
     'short compressed 8-bit' ],
   [ 'bad8wid0.bmp', 'Image sizes must be positive',
     'width 0 (8-bit)' ],
   [ 'bad8oflow.bmp', 'integer overflow calculating image allocation',
     'overflow integers on 32-bit machines (8-bit)', '32bitonly' ],

   # 24-bit/pixel BMPs
   [ 'short24.bmp', 'failed reading image data',
     'short 24-bit' ],
   [ 'bad24wid0.bmp', 'Image sizes must be positive',
     'width 0 (24-bit)' ],
   [ 'bad24oflow.bmp', 'integer overflow calculating image allocation',
     'overflow integers on 32-bit machines (24-bit)', '32bitonly' ],
   [ 'bad24comp.bmp', 'unknown 24-bit BMP compression (4)',
     'bad compression (24-bit)' ],
  );
use Config;
my $intsize = $Config{intsize};
for my $test (@tests) {
  my ($file, $error, $comment, $bit32only) = @$test;
  if (!$bit32only || $intsize == 4) {
    okn($test_num++, !$imoo->read(file=>"testimg/$file"), $comment);
    isn($test_num++, $imoo->errstr, $error, "check error message");
  }
  else {
    skipn($test_num, 2, "only tested on 32-bit machines");
    $test_num += 2;
  }
}

# previously we didn't seek to the offbits position before reading
# the image data, check we handle it correctly
# in each case the first is an original image with a given number of
# bits and the second is the same file with data inserted before the
# image bits and the offset modified to suit
my @comp =
  (
   [ 'winrgb2.bmp', 'winrgb2off.bmp', 1 ],
   [ 'winrgb4.bmp', 'winrgb4off.bmp', 4 ],
   [ 'winrgb8.bmp', 'winrgb8off.bmp', 8 ],
   [ 'winrgb24.bmp', 'winrgb24off.bmp', 24 ],
  );

for my $comp (@comp) {
  my ($base_file, $off_file, $bits) = @$comp;

  my $base_im = Imager->new;
  my $got_base = 
    okn($test_num++, $base_im->read(file=>"testimg/$base_file"),
        "read original")
      or print "# ",$base_im->errstr,"\n";
  my $off_im = Imager->new;
  my $got_off =
    okn($test_num++, $off_im->read(file=>"testimg/$off_file"),
        "read offset file")
      or print "# ",$off_im->errstr,"\n";
  if ($got_base && $got_off) {
    okn($test_num++, !i_img_diff($base_im->{IMG}, $off_im->{IMG}), 
        "compare base and offset image ($bits bits)");
  }
  else {
    skipn($test_num++, 1, "missed one file");
  }
}
                              
sub write_test {
  my ($test_num, $im, $filename) = @_;
  local *FH;

  if (open FH, "> $filename") {
    binmode FH;
    my $IO = Imager::io_new_fd(fileno(FH));
    if (Imager::i_writebmp_wiol($im, $IO)) {
      print "ok $test_num\n";
    }
    else {
      print "not ok $test_num # ",Imager->_error_as_msg(),"\n";
    }
    undef $IO;
    close FH;
  }
  else {
    print "not ok $test_num # $!\n";
  }
}

sub read_test {
  my ($test_num, $filename, $im, %tags) = @_;
  local *FH;
  
  print "# read_test: $filename\n";

  $tags{i_format} = "bmp";

  if (open FH, "< $filename") {
    binmode FH;
    my $IO = Imager::io_new_fd(fileno(FH));
    my $im_read = Imager::i_readbmp_wiol($IO);
    if ($im_read) {
      my $diff = i_img_diff($im, $im_read);
      if ($diff > $base_diff) {
        print "not ok $test_num # image mismatch $diff\n";
      }
      else {
        my $tags_ok = 1;
        for my $tag (keys %tags) {
          if (my $index = Imager::i_tags_find($im_read, $tag, 0)) {
            my ($name, $value) = Imager::i_tags_get($im_read, $index);
            my $exp_value = $tags{$tag};
            print "#   tag $name = '$value' - expect '$exp_value'\n";
            if ($exp_value =~ /\d/) {
              if ($value != $tags{$tag}) {
                print "# tag $tag value mismatch $tags{$tag} != $value\n";
                $tags_ok = 0;
              }
            }
            else {
              if ($value ne $tags{$tag}) {
                print "# tag $tag value mismatch $tags{$tag} != $value\n";
                $tags_ok = 0;
              }
            }
          }
        }
        if ($tags_ok) {
          print "ok $test_num\n";
        }
        else {
          print "not ok $test_num # bad tag values\n";
        }
        #  for my $i (0 .. Imager::i_tags_count($im_read)-1) {
        #    my ($name, $value) = Imager::i_tags_get($im_read, $i);
        #    print "# tag '$name' => '$value'\n";
        #}
      }
    }
    else {
      print "not ok $test_num # ",Imager->_error_as_msg(),"\n";
    }
    undef $IO;
    close FH;
  }
  else {
    print "not ok $test_num # $!\n";
  }
}

