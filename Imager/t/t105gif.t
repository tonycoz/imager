$|=1;
print "1..26\n";
use Imager qw(:all);

init_log("testout/t105gif.log",1);

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

if (!i_has_format("gif")) {
	for (1..26) { print "ok $_ # skip no gif support\n"; }
} else {
    open(FH,">testout/t105.gif") || die "Cannot open testout/t105.gif\n";
    binmode(FH);
    i_writegifmc($img,fileno(FH),7) || die "Cannot write testout/t105.gif\n";
    close(FH);

    print "ok 1\n";

    open(FH,"testout/t105.gif") || die "Cannot open testout/t105.gif\n";
    binmode(FH);
    $img=i_readgif(fileno(FH)) || die "Cannot read testout/t105.gif\n";
    close(FH);

    print "ok 2\n";

    open(FH,"testout/t105.gif") || die "Cannot open testout/t105.gif\n";
    binmode(FH);
    ($img, $palette)=i_readgif(fileno(FH));
    $img || die "Cannot read testout/t105.gif\n";
    close(FH);

    $palette=''; # just to skip a warning.

    print "ok 3\n";
    
    # check that reading interlaced/non-interlaced versions of 
    # the same GIF produce the same image
    # I could replace this with code that used Imager's built-in
    # image comparison code, but I know this code revealed the error
    open(FH, "<testimg/scalei.gif") || die "Cannot open testimg/scalei.gif";
    binmode FH;
    ($imgi) = i_readgif(fileno(FH));
    $imgi || die "Cannot read testimg/scalei.gif";
    close FH;
    print "ok 4\n";
    open FH, "<testimg/scale.gif" or die "Cannot open testimg/scale.gif";
    binmode FH;
    ($imgni) = i_readgif(fileno(FH));
    $imgni or die "Cannot read testimg/scale.gif";
    close FH;
    print "ok 5\n";

    open FH, ">testout/t105i.ppm" or die "Cannot create testout/t105i.ppm";
    binmode FH;
    i_writeppm($imgi, fileno(FH)) or die "Cannot write testout/t105i.ppm";
    close FH;

    open FH, ">testout/t105ni.ppm" or die "Cannot create testout/t105ni.ppm";
    binmode FH;
    i_writeppm($imgni, fileno(FH)) or die "Cannot write testout/t105ni.ppm";
    close FH;

    # compare them
    open FH, "<testout/t105i.ppm" or die "Cannot open testout/t105i.ppm";
    $datai = do { local $/; <FH> };
    close FH;
    open FH, "<testout/t105ni.ppm" or die "Cannot open testout/t105ni.ppm";
    $datani = do { local $/; <FH> };
    close FH;
    if ($datai eq $datani) {
      print "ok 6\n";
    }
    else {
      print "not ok 6\n";
    }

    my $gifver = Imager::i_giflib_version();
    if ($gifver >= 4.0) {
      # reading with a callback
      # various sizes to make sure the buffering works
      # requested size
      open FH, "<testimg/scale.gif" or die "Cannot open testimg/scale.gif";
      binmode FH;
      # no callback version in giflib3, so don't overwrite a good image
      my $img2 = i_readgif_callback(sub { my $tmp; read(FH, $tmp, $_[0]) and $tmp });
      close FH; 
      print $img ? "ok 7\n" : "not ok 7\n";
      
      print test_readgif_cb(1) ? "ok 8\n" : "not ok 8\n";
      print test_readgif_cb(512) ? "ok 9\n" : "not ok 9\n";
      print test_readgif_cb(1024) ? "ok 10\n" : "not ok 10\n";
    }
    else {
      for (7..10) {
	print "ok $_ # skip giflib3 doesn't support callbacks\n";
      }
    }
    open FH, ">testout/t105_mc.gif" or die "Cannot open testout/t105_mc.gif";
    binmode FH;
    i_writegifmc($img, fileno(FH), 7) or print "not ";
    close(FH);
    print "ok 11\n";

    # new writegif_gen
    # test webmap, custom errdiff map
    # (looks fairly awful)
    open FH, ">testout/t105_gen.gif" or die $!;
    binmode FH;
    i_writegif_gen(fileno(FH), { make_colors=>'webmap',
	                         translate=>'errdiff',
				 errdiff=>'custom',
				 errdiff_width=>2,
				 errdiff_height=>2,
				 errdiff_map=>[0, 1, 1, 0]}, $img)
      or print "not ";
    close FH;
    print "ok 12\n";    

    print "# the following tests are fairly slow\n";
    
    # test animation, mc_addi, error diffusion, ordered transparency
    my @imgs;
    my $sortagreen = i_color_new(0, 255, 0, 63);
    for my $i (0..4) {
      my $im = Imager::ImgRaw::new(200, 200, 4);
      for my $j (0..$i-1) {
	my $fill = i_color_new(0, 128, 0, 255 * ($i-$j)/$i);
	i_box_filled($im, 0, $j*40, 199, $j*40+40, $fill);
      }
      i_box_filled($im, 0, $i*40, 199, 199, $blue);
      push(@imgs, $im);
    }
    my @gif_delays = (50) x 5;
    my @gif_disposal = (2) x 5;
    open FH, ">testout/t105_anim.gif" or die $!;
    binmode FH;
    i_writegif_gen(fileno(FH), { make_colors=>'addi',
				 translate=>'closest',
				 gif_delays=>\@gif_delays,
				 gif_disposal=>\@gif_disposal,
				 transp=>'ordered',
				 tr_orddith=>'dot8'}, @imgs)
      or die "Cannot write anim gif";
    close FH;
    print "ok 13\n";

    if ($gifver >= 4.0) {
      unless (fork) {
	# this can SIGSEGV with some versions of giflib
	open FH, ">testout/t105_anim_cb.gif" or die $!;
	i_writegif_callback(sub { 
			      print FH $_[0] 
			    },
			    -1, # max buffering
			    { make_colors=>'webmap',	
			      translate=>'closest',
			      gif_delays=>\@gif_delays,
			      gif_disposal=>\@gif_disposal,
			      #transp=>'ordered',
			      tr_orddith=>'dot8'}, @imgs)
	  or die "Cannot write anim gif";
	close FH;
	print "ok 14\n";
	exit;
      }
      if (wait > 0 && $?) {
	print "not ok 14 # you probably need to patch giflib\n";
	print <<EOS;
#--- egif_lib.c	2000/12/11 07:33:12	1.1
#+++ egif_lib.c	2000/12/11 07:33:48
#@@ -167,6 +167,12 @@
#         _GifError = E_GIF_ERR_NOT_ENOUGH_MEM;
#         return NULL;
#     }
#+    if ((Private->HashTable = _InitHashTable()) == NULL) {
#+        free(GifFile);
#+        free(Private);
#+        _GifError = E_GIF_ERR_NOT_ENOUGH_MEM;
#+        return NULL;
#+    }
#
#     GifFile->Private = (VoidPtr) Private;
#     Private->FileHandle = 0;
EOS
      }
    }
    else {
      print "ok 14 # skip giflib3 doesn't support callbacks\n";
    }
    @imgs = ();
    for $g (0..3) {
      my $im = Imager::ImgRaw::new(200, 200, 3);
      for my $x (0 .. 39) {
	for my $y (0 .. 39) {
	  my $c = i_color_new($x * 6, $y * 6, 32*$g+$x+$y, 255);
	  i_box_filled($im, $x*5, $y*5, $x*5+4, $y*5+4, $c);
	}
      }
      push(@imgs, $im);
    }
    # test giflib with multiple palettes
    # (it was meant to test the NS loop extension too, but that's broken)
    # this looks better with make_colors=>'addi', translate=>'errdiff'
    # this test aims to overload the palette for each image, so the
    # output looks moderately horrible
    open FH, ">testout/t105_mult_pall.gif" or die "Cannot create file: $!";
    binmode FH;
    i_writegif_gen(fileno(FH), { make_colors=>'webmap',
				 translate=>'giflib',
				 gif_delays=>[ 50, 50, 50, 50 ],
				 #gif_loop_count => 50,
				 gif_each_palette => 1,
			       }, @imgs) or print "not ";
    close FH;
    print "ok 15\n";

    # regression test: giflib doesn't like 1 colour images
    my $img1 = Imager::ImgRaw::new(100, 100, 3);
    i_box_filled($img1, 0, 0, 100, 100, $red);
    open FH, ">testout/t105_onecol.gif" or die $!;
    binmode FH;
    if (i_writegif_gen(fileno(FH), { translate=>'giflib'}, $img1)) {
      print "ok 16 # single colour write regression\n";
    } else {
      print "not ok 16 # single colour write regression\n";
    }
    close FH;
    
    # transparency test
    # previously it was harder do write transparent images
    # tests the improvements
    my $timg = Imager::ImgRaw::new(20, 20, 4);
    my $trans = i_color_new(255, 0, 0, 127);
    i_box_filled($timg, 0, 0, 20, 20, $green);
    i_box_filled($timg, 2, 2, 18, 18, $trans);
    open FH, ">testout/t105_trans.gif" or die $!;
    binmode FH;
    i_writegif_gen(fileno(FH), { make_colors=>'addi',
				 translate=>'closest',
				 transp=>'ordered',
			       }, $timg) or print "not ";
    print "ok 17\n";
    close FH;

    # some error handling tests
    # open a file handle for read and try to save to it
    # is this idea portable?
    # whether or not it is, giflib segfaults on this <sigh>
    #open FH, "<testout/t105_trans.gif" or die $!;
    #binmode FH; # habit, I suppose
    #if (i_writegif_gen(fileno(FH), {}, $timg)) {
    #  # this is meant to _fail_
    #  print "not ok 18 # writing to read-only should fail";
    #}
    #else {
    #  print "ok 18 # ",Imager::_error_as_msg(),"\n";
    #}
    #close FH;

    # try to read a file of the wrong format - the script will do
    open FH, "<t/t105gif.t"
      or die "Cannot open this script!: $!";
    binmode FH;
    my $im2 = i_readgif(fileno(FH));
    if ($im2) {
      # this should have failed
      print "not ok 18 # giflib think script if a GIF file\n";
    }
    else {
      print "ok 18 # ",Imager::_error_as_msg(),"\n";
    }
    close FH;

    # try to save no images :)
    open FH, ">testout/t105_none.gif"
      or die "Cannot open testout/t105_none.gif: $!";
    binmode FH;
    if (i_writegif_gen(fileno(FH), {}, "hello")) {
      print "not ok 19 # shouldn't be able to save strings\n";
    }
    else {
      print "ok 19 # ",Imager::_error_as_msg(),"\n";
    }

    # try to read a truncated gif (no image descriptors)
    read_failure('testimg/trimgdesc.gif', 20);
    # file truncated just after the image descriptor tag
    read_failure('testimg/trmiddesc.gif', 21);
    # image has no colour map
    read_failure('testimg/nocmap.gif', 22);

    # image has a local colour map
    open FH, "< testimg/loccmap.gif"
      or die "Cannot open testimg/loccmap.gif: $!";
    binmode FH;
    if (i_readgif(fileno(FH))) {
      print "ok 23\n";
    }
    else {
      print "not ok 23 # failed to read image with only a local colour map";
    }
    close FH;

    # image has global and local colour maps
    open FH, "< testimg/screen2.gif"
      or die "Cannot open testimg/screen2.gif: $!";
    binmode FH;
    my $ims = i_readgif(fileno(FH));
    if ($ims) {
      print "ok 24\n";
    }
    else {
      print "not ok 24 # ",Imager::_error_as_msg(),"\n";
    }
    close FH;
    open FH, "< testimg/expected.gif"
      or die "Cannot open testimg/expected.gif: $!";
    binmode FH;
    my $ime = i_readgif(fileno(FH));
    close FH;
    if ($ime) {
      print "ok 25\n";
    }
    else {
      print "not ok 25 # ",Imager::_error_as_msg(),"\n";
    }
    if ($ims && $ime) {
      if (i_img_diff($ime, $ims)) {
	print "not ok 26 # mismatch ",i_img_diff($ime, $ims),"\n";
	# save the bad one
	open FH, "> testout/t105_screen2.gif"
	  or die "Cannot create testout/t105_screen.gif: $!";
	binmode FH;
	i_writegifmc($ims, fileno(FH), 7)
	  or print "# could not save t105_screen.gif\n";
	close FH;
      }
      else {
	print "ok 26\n";
      }
    }
    else {
      print "ok 26 # skipped\n";
    }
}

sub test_readgif_cb {
  my ($size) = @_;

  open FH, "<testimg/scale.gif" or die "Cannot open testimg/scale.gif";
  binmode FH;
  my $img = i_readgif_callback(sub { my $tmp; read(FH, $tmp, $size) and $tmp });
  close FH; 
  return $img;
}

# tests for reading bad gif files
sub read_failure {
  my ($filename, $testnum) = @_;

  open FH, "< $filename"
    or die "Cannot open $filename: $!";
  binmode FH;
  my ($result, $map) = i_readgif(fileno(FH));
  if ($result) {
    print "not ok $testnum # this is an invalid file, we succeeded\n";
  }
  else {
    print "ok $testnum # ",Imager::_error_as_msg(),"\n";
  }
  close FH;
}

