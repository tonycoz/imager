#!perl -w
print "1..4\n";
use Imager qw(:all);
use strict;
init_log("testout/t107bmp.log",1);

my $green=i_color_new(0,255,0,255);
my $blue=i_color_new(0,0,255,255);
my $red=i_color_new(255,0,0,255);

my $img=Imager::ImgRaw::new(150,150,3);
my $cmpimg=Imager::ImgRaw::new(150,150,3);

i_box_filled($img,70,25,130,125,$green);
i_box_filled($img,20,25,80,125,$blue);
i_arc($img,75,75,30,0,361,$red);
i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

write_test(1, $img, "testout/t107_24bit.bmp");
# 'webmap' is noticably faster than the default
my $im8 = Imager::i_img_to_pal($img, { make_colors=>'webmap', 
				       translate=>'errdiff'});
write_test(2, $im8, "testout/t107_8bit.bmp");
my $im4 = Imager::i_img_to_pal($img, { max_colors=>16 });
write_test(3, $im4, "testout/t107_4bit.bmp");
my $im1 = Imager::i_img_to_pal($img, { colors=>[ NC(0, 0, 0), NC(176, 160, 144) ],
			       make_colors=>'none', translate=>'errdiff' });
write_test(4, $im1, "testout/t107_1bit.bmp");

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
