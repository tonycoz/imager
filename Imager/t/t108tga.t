#!perl -w
print "1..16\n";
use Imager qw(:all);
use strict;
init_log("testout/t108tga.log",1);


my $img = create_test_image();
my $base_diff = 0;

write_test(1, $img, "testout/t108_24bit.tga", 0, 0, "");
write_test(2, $img, "testout/t108_24bit_rle.tga", 0, 1, "");
write_test(3, $img, "testout/t108_15bit.tga", 1, 1, "");
write_test(4, $img, "testout/t108_15bit_rle.tga", 1, 1, "");

# 'webmap' is noticably faster than the default
my $im8 = Imager::i_img_to_pal($img, { make_colors=>'webmap',
				       translate=>'errdiff'});

write_test(5, $im8, "testout/t108_8bit.tga", 0, 0, "");
write_test(6, $im8, "testout/t108_8bit_rle.tga", 0, 1, "");
write_test(7, $im8, "testout/t108_8_15bit.tga", 1, 0, "");
write_test(8, $im8, "testout/t108_8_15bit_rle.tga", 1, 1, "");


# use a fixed palette so we get reproducible results for the compressed
# version

my @bit4 = map { NC($_) }
  qw(605844 966600 0148b2 00f800 bf0a33 5e009e
     2ead1b 0000f8 004b01 fd0000 0e1695 000002);

my @bit1 = (NC(0, 0, 0), NC(176, 160, 144));

my $im4 = Imager::i_img_to_pal($img, { colors=>\@bit4,
				       make_colors=>'none' });

my $im1 = Imager::i_img_to_pal($img, { colors=>\@bit1,
				       make_colors=>'none',
				       translate=>'errdiff' });

write_test(9, $im4, "testout/t108_4bit.tga", 0, 1, "");
write_test(10, $im1, "testout/t108_1bit.tga", 0, 1, "This is a comment!");

read_test(11, "testout/t108_24bit.tga", $img);
read_test(12, "testout/t108_8bit.tga",  $im8);
read_test(13, "testout/t108_4bit.tga",  $im4);
read_test(14, "testout/t108_1bit.tga",  $im1);

# the following might have slight differences

$base_diff = i_img_diff($img, $im8) * 2;

print "# base difference $base_diff\n";

my $imoo = Imager->new;
if ($imoo->read(file=>'testout/t108_24bit.tga')) {
  print "ok 15\n";
} else {
  print "not ok 15 # ",$imoo->errstr,"\n";
}

if ($imoo->write(file=>'testout/t108_oo.tga')) {
  print "ok 16\n";
} else {
  print "not ok 16 # ",$imoo->errstr,"\n";
}




sub write_test {
  my ($test_num, $im, $filename, $wierdpack, $compress, $idstring) = @_;
  local *FH;

  if (open FH, "> $filename") {
    binmode FH;
    my $IO = Imager::io_new_fd(fileno(FH));
    if (Imager::i_writetga_wiol($im, $IO, $wierdpack, $compress, $idstring)) {
      print "ok $test_num\n";
    } else {
      print "not ok $test_num # ",Imager->_error_as_msg(),"\n";
    }
    undef $IO;
    close FH;
  } else {
    print "not ok $test_num # $!\n";
  }
}


sub read_test {
  my ($test_num, $filename, $im, %tags) = @_;
  local *FH;

  if (open FH, "< $filename") {
    binmode FH;
    my $IO = Imager::io_new_fd(fileno(FH));
    my $im_read = Imager::i_readtga_wiol($IO,-1);
    if ($im_read) {
      my $diff = i_img_diff($im, $im_read);
      if ($diff > $base_diff) {
        print "not ok $test_num # image mismatch $diff\n";
      }
      print "ok $test_num\n";
    } else {
      print "not ok $test_num # ",Imager->_error_as_msg(),"\n";
    }
    undef $IO;
    close FH;
  } else {
    print "not ok $test_num # $!\n";
  }
}



sub create_test_image {

  my $green  = i_color_new(0,255,0,255);
  my $blue   = i_color_new(0,0,255,255);
  my $red    = i_color_new(255,0,0,255);

  my $img    = Imager::ImgRaw::new(150,150,3);

  i_box_filled($img, 70, 25, 130, 125, $green);
  i_box_filled($img, 20, 25,  80, 125, $blue);
  i_arc($img, 75, 75, 30, 0, 361, $red);
  i_conv($img, [0.1, 0.2, 0.4, 0.2, 0.1]);

  return $img;
}
