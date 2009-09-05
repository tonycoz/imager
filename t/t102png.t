#!perl -w
use strict;
use Imager qw(:all);
use Test::More;
use Imager::Test qw(test_image_raw);

init_log("testout/t102png.log",1);

i_has_format("png")
  or skip_all("No png support");

plan tests => 33;

my $green  = i_color_new(0,   255, 0,   255);
my $blue   = i_color_new(0,   0,   255, 255);
my $red    = i_color_new(255, 0,   0,   255);

my $img    = test_image_raw();

my $timg = Imager::ImgRaw::new(20, 20, 4);
my $trans = i_color_new(255, 0, 0, 127);
i_box_filled($timg, 0, 0, 20, 20, $green);
i_box_filled($timg, 2, 2, 18, 18, $trans);

Imager::i_tags_add($img, "i_xres", 0, "300", 0);
Imager::i_tags_add($img, "i_yres", 0, undef, 200);
# the following confuses the GIMP
#Imager::i_tags_add($img, "i_aspect_only", 0, undef, 1);
open(FH,">testout/t102.png") || die "cannot open testout/t102.png for writing\n";
binmode(FH);
my $IO = Imager::io_new_fd(fileno(FH));
ok(i_writepng_wiol($img, $IO), "write");
close(FH);

open(FH,"testout/t102.png") || die "cannot open testout/t102.png\n";
binmode(FH);
$IO = Imager::io_new_fd(fileno(FH));
my $cmpimg = i_readpng_wiol($IO, -1);
close(FH);
ok($cmpimg, "read png");

print "# png average mean square pixel difference: ",sqrt(i_img_diff($img,$cmpimg))/150*150,"\n";
is(i_img_diff($img, $cmpimg), 0, "compare saved and original images");

my %tags = map { Imager::i_tags_get($cmpimg, $_) }
  0..Imager::i_tags_count($cmpimg) - 1;
ok(abs($tags{i_xres} - 300) < 1, "i_xres: $tags{i_xres}");
ok(abs($tags{i_yres} - 200) < 1, "i_yres: $tags{i_yres}");
is($tags{i_format}, "png", "i_format: $tags{i_format}");

open FH, "> testout/t102_trans.png"
  or die "Cannot open testout/t102_trans.png: $!";
binmode FH;
$IO = Imager::io_new_fd(fileno(FH));
ok(i_writepng_wiol($timg, $IO), "write tranparent");
close FH;

open FH,"testout/t102_trans.png" 
  or die "cannot open testout/t102_trans.png\n";
binmode(FH);
$IO = Imager::io_new_fd(fileno(FH));
$cmpimg = i_readpng_wiol($IO, -1);
ok($cmpimg, "read transparent");
close(FH);

print "# png average mean square pixel difference: ",sqrt(i_img_diff($timg,$cmpimg))/150*150,"\n";
is(i_img_diff($timg, $cmpimg), 0, "compare saved and original transparent");

# REGRESSION TEST
# png.c 1.1 would produce an incorrect image when loading images with
# less than 8 bits/pixel with a transparent palette entry
open FH, "< testimg/palette.png"
  or die "cannot open testimg/palette.png: $!\n";
binmode FH;
$IO = Imager::io_new_fd(fileno(FH));
# 1.1 may segfault here (it does with libefence)
my $pimg = i_readpng_wiol($IO,-1);
ok($pimg, "read transparent paletted image");
close FH;

open FH, "< testimg/palette_out.png"
  or die "cannot open testimg/palette_out.png: $!\n";
binmode FH;
$IO = Imager::io_new_fd(fileno(FH));
my $poimg = i_readpng_wiol($IO, -1);
ok($poimg, "read palette_out image");
close FH;
if (!is(i_img_diff($pimg, $poimg), 0, "images the same")) {
  print <<EOS;
# this tests a bug in Imager's png.c v1.1
# if also tickles a bug in libpng before 1.0.5, so you may need to
# upgrade libpng
EOS
}

{ # check file limits are checked
  my $limit_file = "testout/t102.png";
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

{ # check if the read_multi fallback works
  my @imgs = Imager->read_multi(file => 'testout/t102.png');
  is(@imgs, 1, "check the image was loaded");
  is(i_img_diff($img, $imgs[0]), 0, "check image matches");
  
  # check the write_multi fallback
  ok(Imager->write_multi({ file => 'testout/t102m.png', type => 'png' }, 
			 @imgs),
       'test write_multi() callback');
  
  # check that we fail if we actually write 2
  ok(!Imager->write_multi({ file => 'testout/t102m.png', type => 'png' }, 
			   @imgs, @imgs),
     'test write_multi() callback failure');
}

{
  ok(grep($_ eq 'png', Imager->read_types), "check png in read types");
  ok(grep($_ eq 'png', Imager->write_types), "check png in write types");
}

