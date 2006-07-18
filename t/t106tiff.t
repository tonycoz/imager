#!perl -w
use strict;
use lib 't';
use Test::More tests => 127;
use Imager qw(:all);
$^W=1; # warnings during command-line tests
$|=1;  # give us some progress in the test harness
init_log("testout/t106tiff.log",1);

my $green=i_color_new(0,255,0,255);
my $blue=i_color_new(0,0,255,255);
my $red=i_color_new(255,0,0,255);

my $img=Imager::ImgRaw::new(150,150,3);

i_box_filled($img,70,25,130,125,$green);
i_box_filled($img,20,25,80,125,$blue);
i_arc($img,75,75,30,0,361,$red);
i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

my $timg = Imager::ImgRaw::new(20, 20, 4);
my $trans = i_color_new(255, 0, 0, 127);
i_box_filled($timg, 0, 0, 20, 20, $green);
i_box_filled($timg, 2, 2, 18, 18, $trans);

SKIP:
{
  unless (i_has_format("tiff")) {
    my $im = Imager->new;
    ok(!$im->read(file=>"testimg/comp4.tif"), "should fail to read tif");
    is($im->errstr, "format 'tiff' not supported", "check no tiff message");
    $im = Imager->new(xsize=>2, ysize=>2);
    ok(!$im->write(file=>"testout/notiff.tif"), "should fail to write tiff");
    is($im->errstr, 'format not supported', "check no tiff message");
    skip("no tiff support", 123);
  }

  Imager::i_tags_add($img, "i_xres", 0, "300", 0);
  Imager::i_tags_add($img, "i_yres", 0, undef, 250);
  # resolutionunit is centimeters
  Imager::i_tags_add($img, "tiff_resolutionunit", 0, undef, 3);
  Imager::i_tags_add($img, "tiff_software", 0, "t106tiff.t", 0);
  open(FH,">testout/t106.tiff") || die "cannot open testout/t106.tiff for writing\n";
  binmode(FH); 
  my $IO = Imager::io_new_fd(fileno(FH));
  ok(i_writetiff_wiol($img, $IO), "write low level");
  close(FH);

  open(FH,"testout/t106.tiff") or die "cannot open testout/t106.tiff\n";
  binmode(FH);
  $IO = Imager::io_new_fd(fileno(FH));
  my $cmpimg = i_readtiff_wiol($IO, -1);
  ok($cmpimg, "read low-level");

  close(FH);

  print "# tiff average mean square pixel difference: ",sqrt(i_img_diff($img,$cmpimg))/150*150,"\n";

  ok(!i_img_diff($img, $cmpimg), "compare written and read image");

  # check the tags are ok
  my %tags = map { Imager::i_tags_get($cmpimg, $_) }
    0 .. Imager::i_tags_count($cmpimg) - 1;
  ok(abs($tags{i_xres} - 300) < 0.5, "i_xres in range");
  ok(abs($tags{i_yres} - 250) < 0.5, "i_yres in range");
  is($tags{tiff_resolutionunit}, 3, "tiff_resolutionunit");
  is($tags{tiff_software}, 't106tiff.t', "tiff_software");
  is($tags{tiff_photometric}, 2, "tiff_photometric"); # PHOTOMETRIC_RGB is 2
  is($tags{tiff_bitspersample}, 8, "tiff_bitspersample");

  $IO = Imager::io_new_bufchain();
  
  ok(Imager::i_writetiff_wiol($img, $IO), "write to buffer chain");
  my $tiffdata = Imager::io_slurp($IO);

  open(FH,"testout/t106.tiff");
  binmode FH;
  my $odata;
  { local $/;
    $odata = <FH>;
  }
  
  is($odata, $tiffdata, "same data in file as in memory");

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
  ok(i_writetiff_wiol_faxable($faximg, $IO, 1), "write faxable, low level");
  close FH;

  # test the OO interface
  my $ooim = Imager->new;
  ok($ooim->read(file=>'testout/t106.tiff'), "read OO");
  ok($ooim->write(file=>'testout/t106_oo.tiff'), "write OO");

  # OO with the fax image
  my $oofim = Imager->new;
  ok($oofim->read(file=>'testout/t106tiff_fax.tiff'),
     "read fax OO");

  # this should have tags set for the resolution
  %tags = map @$_, $oofim->tags;
  is($tags{i_xres}, 204, "fax i_xres");
  is($tags{i_yres}, 196, "fax i_yres");
  ok(!$tags{i_aspect_only}, "i_aspect_only");
  # resunit_inches
  is($tags{tiff_resolutionunit}, 2, "tiff_resolutionunit");
  is($tags{tiff_bitspersample}, 1, "tiff_bitspersample");
  is($tags{tiff_photometric}, 0, "tiff_photometric");

  ok($oofim->write(file=>'testout/t106_oo_fax.tiff', class=>'fax'),
     "write OO, faxable");

  # the following should fail since there's no type and no filename
  my $oodata;
  ok(!$ooim->write(data=>\$oodata), "write with no type and no filename to guess with");

  # OO to data
  ok($ooim->write(data=>\$oodata, type=>'tiff'), "write to data")
    or print "# ",$ooim->errstr, "\n";
  is($oodata, $tiffdata, "check data matches between memory and file");

  # make sure we can write non-fine mode
  ok($oofim->write(file=>'testout/t106_oo_faxlo.tiff', class=>'fax', fax_fine=>0), "write OO, fax standard mode");

  # paletted reads
  my $img4 = Imager->new;
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
  my $diff = i_img_diff($img4->{IMG}, $bmp4->{IMG});
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

  # writing even more images to tiff - we weren't handling more than five
  # correctly on read
  @imgs = map $ooim->copy(), 1..40;
  $rc = Imager->write_multi({file=>'testout/t106_multi2.tif'}, @imgs);
  ok($rc, "writing 40 images to tiff");
  @out = Imager->read_multi(file=>'testout/t106_multi2.tif');
  ok(@imgs == @out, "reading 40 images from tiff");
  # force some allocation activity - helps crash here if it's the problem
  @out = @imgs = ();

  # multi-image fax files
  ok(Imager->write_multi({file=>'testout/t106_faxmulti.tiff', class=>'fax'},
                         $oofim, $oofim), "write multi fax image");
  @imgs = Imager->read_multi(file=>'testout/t106_faxmulti.tiff');
  ok(@imgs == 2, "reading multipage fax");
  ok(Imager::i_img_diff($imgs[0]{IMG}, $oofim->{IMG}) == 0,
     "compare first fax image");
  ok(Imager::i_img_diff($imgs[1]{IMG}, $oofim->{IMG}) == 0,
     "compare second fax image");

  my ($format) = $imgs[0]->tags(name=>'i_format');
  ok(defined $format && $format eq 'tiff', "check i_format tag");

  my $unit = $imgs[0]->tags(name=>'tiff_resolutionunit');
  ok(defined $unit && $unit == 2, "check tiff_resolutionunit tag");
  my $unitname = $imgs[0]->tags(name=>'tiff_resolutionunit_name');
  ok(defined $unitname && $unitname eq 'inch', 
     "check tiff_resolutionunit_name tag");

  my $warned = Imager->new;
  ok($warned->read(file=>"testimg/tiffwarn.tif"), "read tiffwarn.tif");
  my ($warning) = $warned->tags(name=>'i_warning');
  ok(defined $warning && $warning =~ /unknown field with tag 28712/,
     "check that warning tag set and correct");

  { # support for reading a given page
    # first build a simple test image
    my $im1 = Imager->new(xsize=>50, ysize=>50);
    $im1->box(filled=>1, color=>$blue);
    $im1->addtag(name=>'tiff_pagename', value => "Page One");
    my $im2 = Imager->new(xsize=>60, ysize=>60);
    $im2->box(filled=>1, color=>$green);
    $im2->addtag(name=>'tiff_pagename', value=>"Page Two");

    # read second page
    my $page_file = 'testout/t106_pages.tif';
    ok(Imager->write_multi({ file=> $page_file}, $im1, $im2),
       "build simple multiimage for page tests");
    my $imwork = Imager->new;
    ok($imwork->read(file=>$page_file, page=>1),
       "read second page");
    is($im2->getwidth, $imwork->getwidth, "check width");
    is($im2->getwidth, $imwork->getheight, "check height");
    is(i_img_diff($imwork->{IMG}, $im2->{IMG}), 0,
       "check image content");
    my ($page_name) = $imwork->tags(name=>'tiff_pagename');
    is($page_name, 'Page Two', "check tag we set");

    # try an out of range page
    ok(!$imwork->read(file=>$page_file, page=>2),
       "check out of range page");
    is($imwork->errstr, "could not switch to page 2", "check message");
  }

  { # test writing returns an error message correctly
    # open a file read only and try to write to it
    open TIFF, "> testout/t106_empty.tif" or die;
    close TIFF;
    open TIFF, "< testout/t106_empty.tif"
      or skip "Cannot open testout/t106_empty.tif for reading", 8;
    binmode TIFF;
    my $im = Imager->new(xsize=>100, ysize=>100);
    ok(!$im->write(fh => \*TIFF, type=>'tiff'),
       "fail to write to read only handle");
    cmp_ok($im->errstr, '=~', 'Could not create TIFF object: Error writing TIFF header: write\(\)',
	   "check error message");
    ok(!Imager->write_multi({ type => 'tiff', fh => \*TIFF }, $im),
       "fail to write multi to read only handle");
    cmp_ok(Imager->errstr, '=~', 'Could not create TIFF object: Error writing TIFF header: write\(\)',
	   "check error message");
    ok(!$im->write(fh => \*TIFF, type=>'tiff', class=>'fax'),
       "fail to write to read only handle (fax)");
    cmp_ok($im->errstr, '=~', 'Could not create TIFF object: Error writing TIFF header: write\(\)',
	   "check error message");
    ok(!Imager->write_multi({ type => 'tiff', fh => \*TIFF, class=>'fax' }, $im),
       "fail to write multi to read only handle (fax)");
    cmp_ok(Imager->errstr, '=~', 'Could not create TIFF object: Error writing TIFF header: write\(\)',
	   "check error message");
  }

  { # test reading returns an error correctly - use test script as an
    # invalid TIFF file
    my $im = Imager->new;
    ok(!$im->read(file=>'t/t106tiff.t', type=>'tiff'),
       "fail to read script as image");
    # we get different magic number values depending on the platform
    # byte ordering
    cmp_ok($im->errstr, '=~',
	   "Error opening file: Not a TIFF (?:or MDI )?file, bad magic number (8483 \\(0x2123\\)|8993 \\(0x2321\\))", 
	   "check error message");
    my @ims = Imager->read_multi(file =>'t/t106tiff.t', type=>'tiff');
    ok(!@ims, "fail to read_multi script as image");
    cmp_ok($im->errstr, '=~',
	   "Error opening file: Not a TIFF (?:or MDI )?file, bad magic number (8483 \\(0x2123\\)|8993 \\(0x2321\\))", 
       "check error message");
  }

  { # write_multi to data
    my $data;
    my $im = Imager->new(xsize => 50, ysize => 50);
    ok(Imager->write_multi({ data => \$data, type=>'tiff' }, $im, $im),
       "write multi to in memory");
    ok(length $data, "make sure something written");
    my @im = Imager->read_multi(data => $data);
    is(@im, 2, "make sure we can read it back");
    is(Imager::i_img_diff($im[0]{IMG}, $im->{IMG}), 0,
       "check first image");
    is(Imager::i_img_diff($im[1]{IMG}, $im->{IMG}), 0,
       "check second image");
  }

  { # handling of an alpha channel for various images
    my $photo_rgb = 2;
    my $photo_cmyk = 5;
    my $photo_cielab = 8;
    my @alpha_images =
      (
       [ 'srgb.tif',    3, $photo_rgb ],
       [ 'srgba.tif',   4, $photo_rgb  ],
       [ 'srgbaa.tif',  4, $photo_rgb  ],
       [ 'scmyk.tif',   3, $photo_cmyk ],
       [ 'scmyka.tif',  4, $photo_cmyk ],
       [ 'scmykaa.tif', 4, $photo_cmyk ],
       [ 'slab.tif',    3, $photo_cielab ],
      );
    for my $test (@alpha_images) {
      my $im = Imager->new;
      ok($im->read(file => "testimg/$test->[0]"),
	 "read alpha test $test->[0]")
	  or print "# ", $im->errstr, "\n";
      is($im->getchannels, $test->[1], "channels for $test->[0] match");
      is($im->tags(name=>'tiff_photometric'), $test->[2],
	 "photometric for $test->[0] match");
    }
  }
}

