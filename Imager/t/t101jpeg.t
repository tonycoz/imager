use Imager qw(:all);

print "1..2\n";

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
  print "ok 1 # skip no jpeg support\n";
  print "ok 2 # skip no jpeg support\n";
} else {
  open(FH,">testout/t101.jpg") || die "cannot open testout/t101.jpg for writing\n";
  binmode(FH);
  i_writejpeg($img,fileno(FH),30);
  close(FH);

  print "ok 1\n";
  
  open(FH,"testout/t101.jpg") || die "cannot open testout/t101.jpg\n";
  binmode(FH);

  ($cmpimg,undef)=i_readjpeg(fileno(FH));
  close(FH);

  print "# jpeg average mean square pixel difference: ",sqrt(i_img_diff($img,$cmpimg))/150*150,"\n";
  print "ok 2\n";
}
