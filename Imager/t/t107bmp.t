#!perl -w
print "1..12\n";
use Imager qw(:all);
use strict;
init_log("testout/t107bmp.log",1);

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
read_test(5, "testout/t107_24bit.bmp", $img, bmp_compression=>0);
read_test(6, "testout/t107_8bit.bmp", $im8, bmp_compression=>0);
read_test(7, "testout/t107_4bit.bmp", $im4, bmp_compression=>0);
read_test(8, "testout/t107_1bit.bmp", $im1, bmp_compression=>0);
# the following might have slight differences
$base_diff = i_img_diff($img, $im8) * 2;
print "# base difference $base_diff\n";
read_test(9, "testimg/comp4.bmp", $im4, bmp_compression=>$bi_rle4);
read_test(10, "testimg/comp8.bmp", $im8, bmp_compression=>$bi_rle8);

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
            if ($value != $tags{$tag}) {
              print "# tag $tag value mismatch $tags{$tag} != $value\n";
              $tags_ok = 0;
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

