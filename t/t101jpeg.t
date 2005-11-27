#!perl -w
use strict;
use lib 't';
use Imager qw(:all);
use Test::More tests => 24;

init_log("testout/t101jpeg.log",1);

my $green=i_color_new(0,255,0,255);
my $blue=i_color_new(0,0,255,255);
my $red=i_color_new(255,0,0,255);

my $img=Imager::ImgRaw::new(150,150,3);
my $cmpimg=Imager::ImgRaw::new(150,150,3);

i_box_filled($img,70,25,130,125,$green);
i_box_filled($img,20,25,80,125,$blue);
i_arc($img,75,75,30,0,361,$red);
i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

i_has_format("jpeg") && print "# has jpeg\n";
if (!i_has_format("jpeg")) {
  # previously we'd crash if we tried to save/read an image via the OO
  # interface when there was no jpeg support
 SKIP:
  {
    my $im = Imager->new;
    ok(!$im->read(file=>"testimg/base.jpg"), "should fail to read jpeg");
    cmp_ok($im->errstr, '=~', qr/format 'jpeg' not supported/, "check no jpeg message");
    $im = Imager->new(xsize=>2, ysize=>2);
    ok(!$im->write(file=>"testout/nojpeg.jpg"), "should fail to write jpeg");
    cmp_ok($im->errstr, '=~', qr/format not supported/, "check no jpeg message");
    skip("no jpeg support", 5);
  }
} else {
  open(FH,">testout/t101.jpg") || die "cannot open testout/t101.jpg for writing\n";
  binmode(FH);
  my $IO = Imager::io_new_fd(fileno(FH));
  ok(i_writejpeg_wiol($img,$IO,30), "write jpeg low level");
  close(FH);

  open(FH, "testout/t101.jpg") || die "cannot open testout/t101.jpg\n";
  binmode(FH);
  $IO = Imager::io_new_fd(fileno(FH));
  ($cmpimg,undef) = i_readjpeg_wiol($IO);
  close(FH);

  print "$cmpimg\n";
  my $diff = sqrt(i_img_diff($img,$cmpimg))/150*150;
  print "# jpeg average mean square pixel difference: ",$diff,"\n";
  ok($cmpimg, "read jpeg low level");

  ok($diff < 10000, "difference between original and jpeg within bounds");

	Imager::log_entry("Starting 4\n", 1);
  my $imoo = Imager->new;
  ok($imoo->read(file=>'testout/t101.jpg'), "read jpeg OO");
  ok($imoo->write(file=>'testout/t101_oo.jpg'), "write jpeg OO");
	Imager::log_entry("Starting 5\n", 1);
  my $oocmp = Imager->new;
  ok($oocmp->read(file=>'testout/t101_oo.jpg'), "read jpeg OO for comparison");

  $diff = sqrt(i_img_diff($imoo->{IMG},$oocmp->{IMG}))/150*150;
  print "# OO image difference $diff\n";
  ok($diff < 10000, "difference between original and jpeg within bounds");

  # write failure test
  open FH, "< testout/t101.jpg" or die "Cannot open testout/t101.jpg: $!";
  binmode FH;
  ok(!$imoo->write(fd=>fileno(FH), type=>'jpeg'), 'failure handling');
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
}
