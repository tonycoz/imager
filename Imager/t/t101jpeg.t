use Imager qw(:all);

print "1..9\n";

init_log("testout/t101jpeg.log",1);

$green=i_color_new(0,255,0,255);
$blue=i_color_new(0,0,255,255);
$red=i_color_new(255,0,0,255);

$img=Imager::ImgRaw::new(150,150,3);
$cmpimg=Imager::ImgRaw::new(150,150,3);

i_box_filled($img,70,25,130,125,$green);
i_box_filled($img,20,25,80,125,$blue);
i_arc($img,75,75,30,0,361,$red);
i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

i_has_format("jpeg") && print "# has jpeg\n";
if (!i_has_format("jpeg")) {
  for (1..9) {
    print "ok $_ # skip no jpeg support\n";
  }
} else {
  open(FH,">testout/t101.jpg") || die "cannot open testout/t101.jpg for writing\n";
  binmode(FH);
  $IO = Imager::io_new_fd(fileno(FH));
  i_writejpeg_wiol($img,$IO,30);
  close(FH);

  print "ok 1\n";
  
  open(FH, "testout/t101.jpg") || die "cannot open testout/t101.jpg\n";
  binmode(FH);
  $IO = Imager::io_new_fd(fileno(FH));
  ($cmpimg,undef) = i_readjpeg_wiol($IO);
  close(FH);

  print "$cmpimg\n";
  my $diff = sqrt(i_img_diff($img,$cmpimg))/150*150;
  print "# jpeg average mean square pixel difference: ",$diff,"\n";
  print "ok 2\n";

  $diff < 10000 or print "not ";
  print "ok 3\n";

	Imager::log_entry("Starting 4\n", 1);
  my $imoo = Imager->new;
  $imoo->read(file=>'testout/t101.jpg') or print "not ";
  print "ok 4\n";
  $imoo->write(file=>'testout/t101_oo.jpg') or print "not ";
	Imager::log_entry("Starting 5\n", 1);
  print "ok 5\n";
  my $oocmp = Imager->new;
  $oocmp->read(file=>'testout/t101_oo.jpg') or print "not ";
  print "ok 6\n";

  $diff = sqrt(i_img_diff($imoo->{IMG},$oocmp->{IMG}))/150*150;
  print "# OO image difference $diff\n";
  $diff < 10000 or print "not ";
  print "ok 7\n";

  # write failure test
  open FH, "< testout/t101.jpg" or die "Cannot open testout/t101.jpg: $!";
  binmode FH;
  ok(8, !$imoo->write(fd=>fileno(FH), type=>'jpeg'), 'failure handling');
  close FH;
  print "# ",$imoo->errstr,"\n";

  # check that the i_format tag is set
  my @fmt = $imoo->tags(name=>'i_format');
  ok(9, @fmt == 1 && $fmt[0] eq 'jpeg', 'i_format tag');
}

sub ok {
  my ($num, $test, $msg) = @_;

  if ($test) {
    print "ok $num\n";
  }
  else {
    print "not ok $num # $msg\n";
  }
}
