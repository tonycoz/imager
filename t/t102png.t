# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)
use lib qw(blib/lib blib/arch);

BEGIN { $| = 1; print "1..12\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager qw(:all);

$loaded = 1;
print "ok 1\n";

init_log("testout/t102png.log",1);

i_has_format("png") && print "# has png\n";

$green  = i_color_new(0,   255, 0,   255);
$blue   = i_color_new(0,   0,   255, 255);
$red    = i_color_new(255, 0,   0,   255);

$img    = Imager::ImgRaw::new(150, 150, 3);
$cmpimg = Imager::ImgRaw::new(150, 150, 3);

i_box_filled($img, 70, 25, 130, 125, $green);
i_box_filled($img, 20, 25, 80,  125, $blue);
i_arc($img, 75, 75, 30, 0, 361, $red);
i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

my $timg = Imager::ImgRaw::new(20, 20, 4);
my $trans = i_color_new(255, 0, 0, 127);
i_box_filled($timg, 0, 0, 20, 20, $green);
i_box_filled($timg, 2, 2, 18, 18, $trans);

if (!i_has_format("png")) {
  for (2..12) {
    print "ok $_ # skip no png support\n";
  }
} else {
  Imager::i_tags_add($img, "i_xres", 0, "300", 0);
  Imager::i_tags_add($img, "i_yres", 0, undef, 200);
  # the following confuses the GIMP
  #Imager::i_tags_add($img, "i_aspect_only", 0, undef, 1);
  open(FH,">testout/t102.png") || die "cannot open testout/t102.png for writing\n";
  binmode(FH);
  $IO = Imager::io_new_fd(fileno(FH));
  i_writepng_wiol($img, $IO) || print "not ";
  close(FH);

  print "ok 2\n";

  open(FH,"testout/t102.png") || die "cannot open testout/t102.png\n";
  binmode(FH);
  $IO = Imager::io_new_fd(fileno(FH));
  $cmpimg = i_readpng_wiol($IO, -1) || print "not ";
  close(FH);

  print "ok 3\n";
  print "# png average mean square pixel difference: ",sqrt(i_img_diff($img,$cmpimg))/150*150,"\n";
  print i_img_diff($img, $cmpimg)
    ? "not ok 4 # saved image different\n" : "ok 4\n";

  my %tags = map { Imager::i_tags_get($img, $_) }
    0..Imager::i_tags_count($img) - 1;
  abs($tags{i_xres} - 300) < 1 or print "not ";
  print "ok 5 # i_xres: $tags{i_xres}\n";
  abs($tags{i_yres} - 200) < 1 or print "not ";
  print "ok 6 # i_yres: $tags{i_yres}\n";

  open FH, "> testout/t102_trans.png"
    or die "Cannot open testout/t102_trans.png: $!";
  binmode FH;
  $IO = Imager::io_new_fd(fileno(FH));
  if (i_writepng_wiol($timg, $IO)) {
    print "ok 7\n";
  }
  else {
    print "ok 7 # skip - png transparency not yet implemented\n";
  }
  close FH;

  open FH,"testout/t102_trans.png" 
    or die "cannot open testout/t102_trans.png\n";
  binmode(FH);
  $IO = Imager::io_new_fd(fileno(FH));
  $cmpimg = i_readpng_wiol($IO, -1) || print "not ";
  close(FH);

  print "ok 8\n";
  print "# png average mean square pixel difference: ",sqrt(i_img_diff($timg,$cmpimg))/150*150,"\n";
  print i_img_diff($timg, $cmpimg)
	? "not ok 9 # saved image different\n" : "ok 9\n";

  # REGRESSION TEST
  # png.c 1.1 would produce an incorrect image when loading images with
  # less than 8 bits/pixel with a transparent palette entry
  open FH, "< testimg/palette.png"
    or die "cannot open testimg/palette.png: $!\n";
  binmode FH;
  $IO = Imager::io_new_fd(fileno(FH));
  # 1.1 may segfault here (it does with libefence)
  my $pimg = i_readpng_wiol($IO,-1)
    or print "not ";
  print "ok 10\n";
  close FH;

  open FH, "< testimg/palette_out.png"
    or die "cannot open testimg/palette_out.png: $!\n";
  binmode FH;
  $IO = Imager::io_new_fd(fileno(FH));
  my $poimg = i_readpng_wiol($IO, -1)
    or print "not ";
  print "ok 11\n";
  close FH;
  if (i_img_diff($pimg, $poimg)) {
    print <<EOS;
not ok 12 # regression or you may need a more recent libpng
# this tests a bug in Imager's png.c v1.1
# if also tickles a bug in libpng before 1.0.5, so you may need to
# upgrade libpng
EOS
  }
  else {
    print "ok 12\n";
  }
}
