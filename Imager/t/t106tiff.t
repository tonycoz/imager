#!perl -w
print "1..69\n";
use Imager qw(:all);
$^W=1; # warnings during command-line tests
$|=1;  # give us some progress in the test harness
init_log("testout/t106tiff.log",1);

$green=i_color_new(0,255,0,255);
$blue=i_color_new(0,0,255,255);
$red=i_color_new(255,0,0,255);

$img=Imager::ImgRaw::new(150,150,3);

i_box_filled($img,70,25,130,125,$green);
i_box_filled($img,20,25,80,125,$blue);
i_arc($img,75,75,30,0,361,$red);
i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

my $timg = Imager::ImgRaw::new(20, 20, 4);
my $trans = i_color_new(255, 0, 0, 127);
i_box_filled($timg, 0, 0, 20, 20, $green);
i_box_filled($timg, 2, 2, 18, 18, $trans);

my $test_num;

if (!i_has_format("tiff")) {
  for (1..69) {
    print "ok $_ # skip no tiff support\n";
  }
} else {
  Imager::i_tags_add($img, "i_xres", 0, "300", 0);
  Imager::i_tags_add($img, "i_yres", 0, undef, 250);
  # resolutionunit is centimeters
  Imager::i_tags_add($img, "tiff_resolutionunit", 0, undef, 3);
  Imager::i_tags_add($img, "tiff_software", 0, "t106tiff.t", 0);
  open(FH,">testout/t106.tiff") || die "cannot open testout/t106.tiff for writing\n";
  binmode(FH); 
  my $IO = Imager::io_new_fd(fileno(FH));
  i_writetiff_wiol($img, $IO);
  close(FH);

  print "ok 1\n";
  
  open(FH,"testout/t106.tiff") or die "cannot open testout/t106.tiff\n";
  binmode(FH);
  $IO = Imager::io_new_fd(fileno(FH));
  $cmpimg = i_readtiff_wiol($IO, -1);

  close(FH);

  print "# tiff average mean square pixel difference: ",sqrt(i_img_diff($img,$cmpimg))/150*150,"\n";
  print "ok 2\n";

  i_img_diff($img, $cmpimg) and print "not ";
  print "ok 3\n";

  # check the tags are ok
  my %tags = map { Imager::i_tags_get($cmpimg, $_) }
    0 .. Imager::i_tags_count($cmpimg) - 1;
  abs($tags{i_xres} - 300) < 0.5 or print "not ";
  print "ok 4\n";
  abs($tags{i_yres} - 250) < 0.5 or print "not ";
  print "ok 5\n";
  $tags{tiff_resolutionunit} == 3 or print "not ";
  print "ok 6\n";
  $tags{tiff_software} eq 't106tiff.t' or print "not ";
  print "ok 7\n";

  $IO = Imager::io_new_bufchain();
  
  Imager::i_writetiff_wiol($img, $IO) or die "Cannot write to bufferchain\n";
  my $tiffdata = Imager::io_slurp($IO);

  open(FH,"testout/t106.tiff");
  binmode FH;
  my $odata;
  { local $/;
    $odata = <FH>;
  }
  
  if ($odata eq $tiffdata) {
    print "ok 8\n";
  } else {
    print "not ok 8\n";
  }

  # test Micksa's tiff writer
  # a shortish fax page
  my $faximg = Imager::ImgRaw::new(1728, 2000, 1);
  my $black = i_color_new(0,0,0,255);
  my $white = i_color_new(255,255,255,255);
  # vaguely test-patterny
  i_box_filled($faximg, 0, 0, 1728, 2000, $white);
  i_box_filled($faximg, 100,100,1628, 200, $black);
  my $width = 1;
  my $pos = 100;
  while ($width+$pos < 1628) {
    i_box_filled($faximg, $pos, 300, $pos+$width-1, 400, $black);
    $pos += $width + 20;
    $width += 2;
  }
  open FH, "> testout/t106tiff_fax.tiff"
    or die "Cannot create testout/t106tiff_fax.tiff: $!";
  binmode FH;
  $IO = Imager::io_new_fd(fileno(FH));
  i_writetiff_wiol_faxable($faximg, $IO, 1)
    or print "not ";
  print "ok 9\n";
  close FH;

  # test the OO interface
  my $ooim = Imager->new;
  $ooim->read(file=>'testout/t106.tiff')
    or print "not ";
  print "ok 10\n";
  $ooim->write(file=>'testout/t106_oo.tiff')
    or print "not ";
  print "ok 11\n";

  # OO with the fax image
  my $oofim = Imager->new;
  $oofim->read(file=>'testout/t106tiff_fax.tiff')
    or print "not ";
  print "ok 12\n";

  # this should have tags set for the resolution
  %tags = map @$_, $oofim->tags;
  $tags{i_xres} == 204 or print "not ";
  print "ok 13\n";
  $tags{i_yres} == 196 or print "not ";
  print "ok 14\n";
  $tags{i_aspect_only} and print "not ";
  print "ok 15\n";
  # resunit_inches
  $tags{tiff_resolutionunit} == 2 or print "not ";
  print "ok 16\n";

  $oofim->write(file=>'testout/t106_oo_fax.tiff', class=>'fax')
    or print "not ";
  print "ok 17\n";

  # the following should fail since there's no type and no filename
  my $oodata;
  $ooim->write(data=>\$oodata)
    and print "not ";
  print "ok 18\n";

  # OO to data
  $ooim->write(data=>\$oodata, type=>'tiff')
    or print "# ",$ooim->errstr, "\nnot ";
  print "ok 19\n";
  $oodata eq $tiffdata or print "not ";
  print "ok 20\n";

  # make sure we can write non-fine mode
  $oofim->write(file=>'testout/t106_oo_faxlo.tiff', class=>'fax', fax_fine=>0)
    or print "not ";
  print "ok 21\n";

  # paletted reads
  my $img4 = Imager->new;
  $test_num = 22;
  ok($img4->read(file=>'testimg/comp4.tif'), "reading 4-bit paletted");
  ok($img4->type eq 'paletted', "image isn't paletted");
  print "# colors: ", $img4->colorcount,"\n";
  ok($img4->colorcount <= 16, "more than 16 colors!");
  #ok($img4->write(file=>'testout/t106_was4.ppm'),
  #   "Cannot write img4");
  # I know I'm using BMP before it's test, but comp4.tif started life 
  # as comp4.bmp
  my $bmp4 = Imager->new;
  ok($bmp4->read(file=>'testimg/comp4.bmp'), "reading 4-bit bmp!");
  $diff = i_img_diff($img4->{IMG}, $bmp4->{IMG});
  print "# diff $diff\n";
  ok($diff == 0, "image mismatch");
  my $img8 = Imager->new;
  ok($img8->read(file=>'testimg/comp8.tif'), "reading 8-bit paletted");
  ok($img8->type eq 'paletted', "image isn't paletted");
  print "# colors: ", $img8->colorcount,"\n";
  #ok($img8->write(file=>'testout/t106_was8.ppm'),
  #   "Cannot write img8");
  ok($img8->colorcount == 256, "more colors than expected");
  my $bmp8 = Imager->new;
  ok($bmp8->read(file=>'testimg/comp8.bmp'), "reading 8-bit bmp!");
  $diff = i_img_diff($img8->{IMG}, $bmp8->{IMG});
  print "# diff $diff\n";
  ok($diff == 0, "image mismatch");
  my $bad = Imager->new;
  ok($bad->read(file=>'testimg/comp4bad.tif'), "bad image not returned");
  ok(scalar $bad->tags(name=>'i_incomplete'), "incomplete tag not set");
  ok($img8->write(file=>'testout/t106_pal8.tif'), "writing 8-bit paletted");
  my $cmp8 = Imager->new;
  ok($cmp8->read(file=>'testout/t106_pal8.tif'),
     "reading 8-bit paletted");
  #print "# ",$cmp8->errstr,"\n";
  ok($cmp8->type eq 'paletted', "pal8 isn't paletted");
  ok($cmp8->colorcount == 256, "pal8 bad colorcount");
  $diff = i_img_diff($img8->{IMG}, $cmp8->{IMG});
  print "# diff $diff\n";
  ok($diff == 0, "written image doesn't match read");
  ok($img4->write(file=>'testout/t106_pal4.tif'), "writing 4-bit paletted");
  ok(my $cmp4 = Imager->new->read(file=>'testout/t106_pal4.tif'),
     "reading 4-bit paletted");
  ok($cmp4->type eq 'paletted', "pal4 isn't paletted");
  ok($cmp4->colorcount == 16, "pal4 bad colorcount");
  $diff = i_img_diff($img4->{IMG}, $cmp4->{IMG});
  print "# diff $diff\n";
  ok($diff == 0, "written image doesn't match read");

  my $work;
  my $seekpos;
  sub io_writer {
    my ($what) = @_;
    if ($seekpos > length $work) {
      $work .= "\0" x ($seekpos - length $work);
    }
    substr($work, $seekpos, length $what) = $what;
    $seekpos += length $what;

    1;
  }
  sub io_reader {
    my ($size, $maxread) = @_;
    #print "io_reader($size, $maxread) pos $seekpos\n";
    my $out = substr($work, $seekpos, $maxread);
    $seekpos += length $out;
    $out;
  }
  sub io_reader2 {
    my ($size, $maxread) = @_;
    #print "io_reader2($size, $maxread) pos $seekpos\n";
    my $out = substr($work, $seekpos, $size);
    $seekpos += length $out;
    $out;
  }
  use IO::Seekable;
  sub io_seeker {
    my ($offset, $whence) = @_;
    #print "io_seeker($offset, $whence)\n";
    if ($whence == SEEK_SET) {
      $seekpos = $offset;
    }
    elsif ($whence == SEEK_CUR) {
      $seekpos += $offset;
    }
    else { # SEEK_END
      $seekpos = length($work) + $offset;
    }
    #print "-> $seekpos\n";
    $seekpos;
  }
  my $did_close;
  sub io_closer {
    ++$did_close;
  }

  # read via cb
  $work = $tiffdata;
  $seekpos = 0;
  my $IO2 = Imager::io_new_cb(undef, \&io_reader, \&io_seeker, undef);
  ok($IO2, "new readcb obj");
  my $img5 = i_readtiff_wiol($IO2, -1);
  ok($img5, "read via cb");
  ok(i_img_diff($img5, $img) == 0, "read from cb diff");

  # read via cb2
  $work = $tiffdata;
  $seekpos = 0;
  my $IO3 = Imager::io_new_cb(undef, \&io_reader2, \&io_seeker, undef);
  ok($IO3, "new readcb2 obj");
  my $img6 = i_readtiff_wiol($IO3, -1);
  ok($img6, "read via cb2");
  ok(i_img_diff($img6, $img) == 0, "read from cb2 diff");

  # write via cb
  $work = '';
  $seekpos = 0;
  my $IO4 = Imager::io_new_cb(\&io_writer, \&io_reader, \&io_seeker,
                              \&io_closer);
  ok($IO4, "new writecb obj");
  ok(i_writetiff_wiol($img, $IO4), "write to cb");
  ok($work eq $odata, "write cb match");
  ok($did_close, "write cb did close");
  open D1, ">testout/d1.tiff" or die;
  print D1 $work;
  close D1;
  open D2, ">testout/d2.tiff" or die;
  print D2 $tiffdata;
  close D2;

  # write via cb2
  $work = '';
  $seekpos = 0;
  $did_close = 0;
  my $IO5 = Imager::io_new_cb(\&io_writer, \&io_reader, \&io_seeker,
                              \&io_closer, 1);
  ok($IO5, "new writecb obj 2");
  ok(i_writetiff_wiol($img, $IO5), "write to cb2");
  ok($work eq $odata, "write cb2 match");
  ok($did_close, "write cb2 did close");

  open D3, ">testout/d3.tiff" or die;
  print D3 $work;
  close D3;

  # multi-image write/read
  my @imgs;
  push(@imgs, map $ooim->copy(), 1..3);
  for my $i (0..$#imgs) {
    $imgs[$i]->addtag(name=>"tiff_pagename", value=>"Page ".($i+1));
  }
  my $rc = Imager->write_multi({file=>'testout/t106_multi.tif'}, @imgs);
  ok($rc, "writing multiple images to tiff");
  my @out = Imager->read_multi(file=>'testout/t106_multi.tif');
  ok(@out == @imgs, "reading multiple images from tiff");
  @out == @imgs or print "# ",scalar @out, " ",Imager->errstr,"\n";
  for my $i (0..$#imgs) {
    ok(i_img_diff($imgs[$i]{IMG}, $out[$i]{IMG}) == 0,
       "comparing image $i");
    my ($tag) = $out[$i]->tags(name=>'tiff_pagename');
    ok($tag eq "Page ".($i+1),
       "tag doesn't match original image");
  }

  # multi-image fax files
  ok(Imager->write_multi({file=>'testout/t106_faxmulti.tiff', class=>'fax'},
                         $oofim, $oofim), "write multi fax image");
  @imgs = Imager->read_multi(file=>'testout/t106_faxmulti.tiff');
  ok(@imgs == 2, "reading multipage fax");
  ok(Imager::i_img_diff($imgs[0]{IMG}, $oofim->{IMG}) == 0,
     "compare first fax image");
  ok(Imager::i_img_diff($imgs[1]{IMG}, $oofim->{IMG}) == 0,
     "compare second fax image");
}

sub ok {
  my ($ok, $msg) = @_;

  if ($ok) {
    print "ok ",$test_num++,"\n";
  }
  else {
    print "not ok ", $test_num++," # line ",(caller)[2]," $msg\n";
  }
}
