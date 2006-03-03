#!perl -w
use strict;
use lib 't';
use Test::More tests => 89;
use Imager qw(:all);
init_log("testout/t107bmp.log",1);
#BEGIN { require 't/testtools.pl'; } # BEGIN to apply prototypes

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
write_test($img, "testout/t107_24bit.bmp");
# 'webmap' is noticably faster than the default
my $im8 = Imager::i_img_to_pal($img, { make_colors=>'webmap', 
				       translate=>'errdiff'});
write_test($im8, "testout/t107_8bit.bmp");
# use a fixed palette so we get reproducible results for the compressed
# version
my @pal16 = map { NC($_) } 
  qw(605844 966600 0148b2 00f800 bf0a33 5e009e
     2ead1b 0000f8 004b01 fd0000 0e1695 000002);
my $im4 = Imager::i_img_to_pal($img, { colors=>\@pal16, make_colors=>'none' });
write_test($im4, "testout/t107_4bit.bmp");
my $im1 = Imager::i_img_to_pal($img, { colors=>[ NC(0, 0, 0), NC(176, 160, 144) ],
			       make_colors=>'none', translate=>'errdiff' });
write_test($im1, "testout/t107_1bit.bmp");
my $bi_rgb = 0;
my $bi_rle8 = 1;
my $bi_rle4 = 2;
my $bi_bitfields = 3;
read_test("testout/t107_24bit.bmp", $img, 
          bmp_compression=>0, bmp_bit_count => 24);
read_test("testout/t107_8bit.bmp", $im8, 
          bmp_compression=>0, bmp_bit_count => 8);
read_test("testout/t107_4bit.bmp", $im4, 
          bmp_compression=>0, bmp_bit_count => 4);
read_test("testout/t107_1bit.bmp", $im1, bmp_compression=>0, 
          bmp_bit_count=>1);
# the following might have slight differences
$base_diff = i_img_diff($img, $im8) * 2;
print "# base difference $base_diff\n";
read_test("testimg/comp4.bmp", $im4, 
          bmp_compression=>$bi_rle4, bmp_bit_count => 4);
read_test("testimg/comp8.bmp", $im8, 
          bmp_compression=>$bi_rle8, bmp_bit_count => 8);

my $imoo = Imager->new;
# read via OO
ok($imoo->read(file=>'testout/t107_24bit.bmp'), "read via OO")
  or print "# ",$imoo->errstr,"\n";

ok($imoo->write(file=>'testout/t107_oo.bmp'), "write via OO")
  or print "# ",$imoo->errstr,"\n";

# various invalid format tests
# we have so many different test images to try to detect all the possible
# failure paths in the code, adding these did detect real problems
print "# catch various types of invalid bmp files\n";
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
   [ 'bad1wid0.bmp', 'file size limit - image width of 0 is not positive',
     'width 0 (1-bit)' ],
   [ 'bad4oflow.bmp', 
     'file size limit - integer overflow calculating storage',
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
   [ 'bad4wid0.bmp', 'file size limit - image width of 0 is not positive',
     'width 0 (4-bit)' ],
   [ 'bad4widbig.bmp', 'file size limit - image width of -2147483628 is not positive',
     'width big (4-bit)' ],
   [ 'bad4oflow.bmp', 'file size limit - integer overflow calculating storage',
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
   [ 'bad8wid0.bmp', 'file size limit - image width of 0 is not positive',
     'width 0 (8-bit)' ],
   [ 'bad8oflow.bmp', 'file size limit - integer overflow calculating storage',
     'overflow integers on 32-bit machines (8-bit)', '32bitonly' ],

   # 24-bit/pixel BMPs
   [ 'short24.bmp', 'failed reading image data',
     'short 24-bit' ],
   [ 'bad24wid0.bmp', 'file size limit - image width of 0 is not positive',
     'width 0 (24-bit)' ],
   [ 'bad24oflow.bmp', 'file size limit - integer overflow calculating storage',
     'overflow integers on 32-bit machines (24-bit)', '32bitonly' ],
   [ 'bad24comp.bmp', 'unknown 24-bit BMP compression (4)',
     'bad compression (24-bit)' ],
  );
use Config;
my $intsize = $Config{intsize};
for my $test (@tests) {
  my ($file, $error, $comment, $bit32only) = @$test;
 SKIP:
  {
    skip("only tested on 32-bit machines", 2)
      if $bit32only && $intsize != 4;
    ok(!$imoo->read(file=>"testimg/$file"), $comment);
    is($imoo->errstr, $error, "check error message");
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
    ok($base_im->read(file=>"testimg/$base_file"),
        "read original")
      or print "# ",$base_im->errstr,"\n";
  my $off_im = Imager->new;
  my $got_off =
    ok($off_im->read(file=>"testimg/$off_file"),
        "read offset file")
      or print "# ",$off_im->errstr,"\n";
 SKIP:
  {
    skip("missed one file", 1)
      unless $got_base && $got_off;
    is(i_img_diff($base_im->{IMG}, $off_im->{IMG}), 0,
        "compare base and offset image ($bits bits)");
  }
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
                              
sub write_test {
  my ($im, $filename) = @_;
  local *FH;

  if (open FH, "> $filename") {
    binmode FH;
    my $IO = Imager::io_new_fd(fileno(FH));
    unless (ok(Imager::i_writebmp_wiol($im, $IO), $filename)) {
      print "# ",Imager->_error_as_msg(),"\n";
    }
    undef $IO;
    close FH;
  }
  else {
    fail("could not open $filename: $!");
  }
}

sub read_test {
  my ($filename, $im, %tags) = @_;
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
	fail("image mismatch reading $filename");
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
        ok($tags_ok, "reading $filename");
        #  for my $i (0 .. Imager::i_tags_count($im_read)-1) {
        #    my ($name, $value) = Imager::i_tags_get($im_read, $i);
        #    print "# tag '$name' => '$value'\n";
        #}
      }
    }
    else {
      fail("could not read $filename: ".Imager->_error_as_msg());
    }
    undef $IO;
    close FH;
  }
  else {
    fail("could not open $filename: $!");
  }
}

