#!perl -w
use strict;
use Imager qw(:all);
use Test::More;
use Imager::Test qw(test_image_raw test_image is_image);

my $debug_writes = 1;

-d "testout" or mkdir "testout";

init_log("testout/t102png.log",1);

$Imager::formats{"png"}
  or plan skip_all => "No png support";

plan tests => 93;

diag("Library version " . Imager::File::PNG::i_png_lib_version());

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
ok(Imager::File::PNG::i_writepng_wiol($img, $IO), "write");
close(FH);

open(FH,"testout/t102.png") || die "cannot open testout/t102.png\n";
binmode(FH);
$IO = Imager::io_new_fd(fileno(FH));
my $cmpimg = Imager::File::PNG::i_readpng_wiol($IO);
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
ok(Imager::File::PNG::i_writepng_wiol($timg, $IO), "write tranparent");
close FH;

open FH,"testout/t102_trans.png" 
  or die "cannot open testout/t102_trans.png\n";
binmode(FH);
$IO = Imager::io_new_fd(fileno(FH));
$cmpimg = Imager::File::PNG::i_readpng_wiol($IO);
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
my $pimg = Imager::File::PNG::i_readpng_wiol($IO);
ok($pimg, "read transparent paletted image");
close FH;

open FH, "< testimg/palette_out.png"
  or die "cannot open testimg/palette_out.png: $!\n";
binmode FH;
$IO = Imager::io_new_fd(fileno(FH));
my $poimg = Imager::File::PNG::i_readpng_wiol($IO);
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

{ # check close failures are handled correctly
  my $im = test_image();
  my $fail_close = sub {
    Imager::i_push_error(0, "synthetic close failure");
    print "# closecb called\n";
    return 0;
  };
  ok(!$im->write(type => "png", callback => sub { 1 },
		 closecb => $fail_close),
     "check failing close fails");
    like($im->errstr, qr/synthetic close failure/,
	 "check error message");
}

{
  ok(grep($_ eq 'png', Imager->read_types), "check png in read types");
  ok(grep($_ eq 'png', Imager->write_types), "check png in write types");
}

{ # read error reporting
  my $im = Imager->new;
  ok(!$im->read(file => "testimg/badcrc.png", type => "png"),
     "read png with bad CRC chunk should fail");
  is($im->errstr, "IHDR: CRC error", "check error message");
}

{ # write error reporting
  my $im = test_image();
  ok(!$im->write(type => "png", callback => limited_write(1), buffered => 0),
     "write limited to 1 byte should fail");
  is($im->errstr, "Write error on an iolayer source.: limit reached",
     "check error message");
}

SKIP:
{ # https://sourceforge.net/tracker/?func=detail&aid=3314943&group_id=5624&atid=105624
  # large images
  Imager::File::PNG::i_png_lib_version() >= 10503
      or skip("older libpng limits image sizes", 12);

  {
    my $im = Imager->new(xsize => 1000001, ysize => 1, channels => 1);
    ok($im, "make a wide image");
    my $data;
    ok($im->write(data => \$data, type => "png"),
       "write wide image as png")
      or diag("write wide: " . $im->errstr);
    my $im2 = Imager->new;
    ok($im->read(data => $data, type => "png"),
       "read wide image as png")
      or diag("read wide: " . $im->errstr);
    is($im->getwidth, 1000001, "check width");
    is($im->getheight, 1, "check height");
    is($im->getchannels, 1, "check channels");
  }

  {
    my $im = Imager->new(xsize => 1, ysize => 1000001, channels => 1);
    ok($im, "make a tall image");
    my $data;
    ok($im->write(data => \$data, type => "png"),
       "write wide image as png")
      or diag("write tall: " . $im->errstr);
    my $im2 = Imager->new;
    ok($im->read(data => $data, type => "png"),
       "read tall image as png")
      or diag("read tall: " . $im->errstr);
    is($im->getwidth, 1, "check width");
    is($im->getheight, 1000001, "check height");
    is($im->getchannels, 1, "check channels");
  }
}

{ # test grayscale read as greyscale
  my $im = Imager->new;
  ok($im->read(file => "testimg/gray.png", type => "png"),
     "read grayscale");
  is($im->getchannels, 1, "check channel count");
  is($im->type, "direct", "check type");
  is($im->bits, 8, "check bits");
  is($im->tags(name => "png_bits"), 8, "check png_bits tag");
  is($im->tags(name => "png_interlace"), 0, "check png_interlace tag");
}

{ # test grayscale + alpha read as greyscale + alpha
  my $im = Imager->new;
  ok($im->read(file => "testimg/graya.png", type => "png"),
     "read grayscale + alpha");
  is($im->getchannels, 2, "check channel count");
  is($im->type, "direct", "check type");
  is($im->bits, 8, "check bits");
  is($im->tags(name => "png_bits"), 8, "check png_bits tag");
  is($im->tags(name => "png_interlace"), 0, "check png_interlace tag");
}

{ # test paletted + alpha read as paletted
  my $im = Imager->new;
  ok($im->read(file => "testimg/paltrans.png", type => "png"),
     "read paletted with alpha");
  is($im->getchannels, 4, "check channel count");
  is($im->type, "paletted", "check type");
  is($im->tags(name => "png_bits"), 8, "check png_bits tag");
  is($im->tags(name => "png_interlace"), 0, "check png_interlace tag");
}

{ # test paletted read as paletted
  my $im = Imager->new;
  ok($im->read(file => "testimg/pal.png", type => "png"),
     "read paletted");
  is($im->getchannels, 3, "check channel count");
  is($im->type, "paletted", "check type");
  is($im->tags(name => "png_bits"), 8, "check png_bits tag");
  is($im->tags(name => "png_interlace"), 0, "check png_interlace tag");
}

{ # test 16-bit rgb read as 16 bit
  my $im = Imager->new;
  ok($im->read(file => "testimg/rgb16.png", type => "png"),
     "read 16-bit rgb");
  is($im->getchannels, 3, "check channel count");
  is($im->type, "direct", "check type");
  is($im->tags(name => "png_interlace"), 0, "check png_interlace tag");
  local $TODO = "Not yet implemented";
  is($im->bits, 16, "check bits");
  is($im->tags(name => "png_bits"), 16, "check png_bits tag");
}

{ # test 1-bit grey read as mono
  my $im = Imager->new;
  ok($im->read(file => "testimg/bilevel.png", type => "png"),
     "read bilevel png");
  is($im->getchannels, 1, "check channel count");
  is($im->tags(name => "png_interlace"), 0, "check png_interlace tag");
  local $TODO = "Not yet implemented";
  is($im->type, "paletted", "check type");
  ok($im->is_bilevel, "should be bilevel");
  is($im->tags(name => "png_bits"), 1, "check png_bits tag");
}

SKIP:
{ # test interlaced read as interlaced and matches original
  my $im_i = Imager->new(file => "testimg/rgb8i.png", filetype => "png");
  ok($im_i, "read interlaced")
    or skip("Could not read rgb8i.png: " . Imager->errstr, 7);
  is($im_i->getchannels, 3, "check channel count");
  is($im_i->type, "direct", "check type");
  is($im_i->tags(name => "png_bits"), 8, "check png_bits");
  is($im_i->tags(name => "png_interlace"), "adam7", "check png_interlace");

  my $im = Imager->new(file => "testimg/rgb8.png", filetype => "png");
  ok($im, "read non-interlaced")
    or skip("Could not read testimg/rgb8.png: " . Imager->errstr, 2);
  is($im->tags(name => "png_interlace"), "0", "check png_interlace");
  is_image($im_i, $im, "compare interlaced and non-interlaced");
}

sub limited_write {
  my ($limit) = @_;

  return
     sub {
       my ($data) = @_;
       $limit -= length $data;
       if ($limit >= 0) {
         print "# write of ", length $data, " bytes successful ($limit left)\n" if $debug_writes;
         return 1;
       }
       else {
         print "# write of ", length $data, " bytes failed\n";
         Imager::i_push_error(0, "limit reached");
         return;
       }
     };
}
