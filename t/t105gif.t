#!perl -w
use strict;
$|=1;
print "1..60\n";
use Imager qw(:all);
require "t/testtools.pl";

my $buggy_giflib_file = "buggy_giflib.txt";

sub ok ($$$);

init_log("testout/t105gif.log",1);

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

if (!i_has_format("gif")) {
  skipn(1, 60, "no gif support");
} else {
    open(FH,">testout/t105.gif") || die "Cannot open testout/t105.gif\n";
    binmode(FH);
    i_writegifmc($img,fileno(FH),6) || die "Cannot write testout/t105.gif\n";
    close(FH);

    print "ok 1\n";

    open(FH,"testout/t105.gif") || die "Cannot open testout/t105.gif\n";
    binmode(FH);
    $img=i_readgif(fileno(FH)) || die "Cannot read testout/t105.gif\n";
    close(FH);

    print "ok 2\n";

    open(FH,"testout/t105.gif") || die "Cannot open testout/t105.gif\n";
    binmode(FH);
    ($img, my $palette)=i_readgif(fileno(FH));
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
    my ($imgi) = i_readgif(fileno(FH));
    $imgi || die "Cannot read testimg/scalei.gif";
    close FH;
    print "ok 4\n";
    open FH, "<testimg/scale.gif" or die "Cannot open testimg/scale.gif";
    binmode FH;
    my ($imgni) = i_readgif(fileno(FH));
    $imgni or die "Cannot read testimg/scale.gif";
    close FH;
    print "ok 5\n";

    open FH, ">testout/t105i.ppm" or die "Cannot create testout/t105i.ppm";
    binmode FH;
    my $IO = Imager::io_new_fd( fileno(FH) );
    i_writeppm_wiol($imgi, $IO) or die "Cannot write testout/t105i.ppm";
    close FH;


    open FH, ">testout/t105ni.ppm" or die "Cannot create testout/t105ni.ppm";
    binmode FH;
    $IO = Imager::io_new_fd( fileno(FH) );
    i_writeppm_wiol($imgni, $IO) or die "Cannot write testout/t105ni.ppm";
    close FH;

    # compare them
    open FH, "<testout/t105i.ppm" or die "Cannot open testout/t105i.ppm";
    my $datai = do { local $/; <FH> };
    close FH;

    open FH, "<testout/t105ni.ppm" or die "Cannot open testout/t105ni.ppm";
    my $datani = do { local $/; <FH> };
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
      _add_tags($im, gif_delay=>50, gif_disposal=>2);
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
				 gif_positions=> [ map [ $_*10, $_*10 ], 0..4 ],
				 gif_user_input=>[ 1, 0, 1, 0, 1 ],
				 transp=>'ordered',
				 tr_orddith=>'dot8'}, @imgs)
      or die "Cannot write anim gif";
    close FH;
    print "ok 13\n";

    my $can_write_callback = 0;
    unlink $buggy_giflib_file;
    if ($gifver >= 4.0) {
      ++$can_write_callback;
      my $good = ext_test(14, <<'ENDOFCODE');
use Imager;
require "t/testtools.pl";
my $timg = test_img();
my @gif_delays = (50) x 5;
my @gif_disposal = (2) x 5;
my @imgs = ($timg) x 5;
open FH, "> testout/t105_anim_cb.gif" or die $!;
binmode FH;
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
ENDOFCODE
      unless ($good) {
        $can_write_callback = 0;
	print "not ok 14 # see $buggy_giflib_file\n";
	print STDERR "\nprobable buggy giflib - skipping tests that depend on a good giflib\n";
	print STDERR "see $buggy_giflib_file for more information\n";
	open FLAG, "> $buggy_giflib_file" or die;
	print FLAG <<EOS;
This file is created by t105gif.t when test 14 fails.

This failure usually indicates you\'re using the original version
of giflib 4.1.0, which has a few bugs that Imager tickles.

You can apply the patch from:

http://www.develop-help.com/imager/giflib.patch

or you can just install Imager as is, if you only need to write GIFs to 
files or file descriptors (such as sockets).

In previous versions of Imager only this test was careful about catching 
the error, we now skip any tests that crashed or failed when the buggy 
giflib was present.
EOS
      }
    }
    else {
      print "ok 14 # skip giflib3 doesn't support callbacks\n";
    }
    @imgs = ();
    my $c = i_color_new(0,0,0,0);
    for my $g (0..3) {
      my $im = Imager::ImgRaw::new(200, 200, 3);
      _add_tags($im, gif_local_map=>1, gif_delay=>150, gif_loop=>10);
      for my $x (0 .. 39) {
	for my $y (0 .. 39) {
          $c->set($x * 6, $y * 6, 32*$g+$x+$y, 255);
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
    if (i_writegif_gen(fileno(FH), { #make_colors=>'webmap',
                                     translate=>'giflib',
                                   }, @imgs)) {
      print "ok 15\n";
    }
    else {
      print "not ok 15 # ", join(":", map $_->[1], Imager::i_errors()),"\n";
    }
    close FH;

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

    unless (-e $buggy_giflib_file) {
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

      # test reading a multi-image file into multiple images
      open FH, "< testimg/screen2.gif"
	or die "Cannot open testimg/screen2.gif: $!";
      binmode FH;
      @imgs = Imager::i_readgif_multi(fileno(FH))
	or print "not ";
      print "ok 27\n";
      close FH;
      @imgs == 2 or print "not ";
      print "ok 28\n";
      for my $img (@imgs) {
	unless (Imager::i_img_type($img) == 1) {
	  print "not ";
	  last;
	}
      }
      print "ok 29\n";
      Imager::i_colorcount($imgs[0]) == 4 or print "not ";
      print "ok 30\n";
      Imager::i_colorcount($imgs[1]) == 2 or print "not ";
      print "ok 31\n";
      Imager::i_tags_find($imgs[0], "gif_left", 0) or print "not ";
      print "ok 32\n";
      my @tags = map {[ Imager::i_tags_get($imgs[1], $_) ]} 0..Imager::i_tags_count($imgs[1])-1;
      my ($left) = grep $_->[0] eq 'gif_left', @tags;
      $left && $left->[1] == 3 or print "not ";
      print "ok 33\n";
      
      # screen3.gif was saved with 
      open FH, "< testimg/screen3.gif"
	or die "Cannot open testimg/screen3.gif: $!";
      binmode FH;
      @imgs = Imager::i_readgif_multi(fileno(FH))
	or print "not ";
      print "ok 34\n";
      close FH;
      eval {
	require 'Data/Dumper.pm';
	Data::Dumper->import();
      };
      unless ($@) {
	# build a big map of all tags for all images
	@tags = 
	  map { 
	    my $im = $_; 
	    [ 
	     map { join ",", map { defined() ? $_ : "undef" } Imager::i_tags_get($im, $_) } 
	     0..Imager::i_tags_count($_)-1 
	    ] 
	  } @imgs;
	my $dump = Dumper(\@tags);
	$dump =~ s/^/# /mg;
	print "# tags from gif\n", $dump;
      }
      
      # at this point @imgs should contain only paletted images
      ok(35, Imager::i_img_type($imgs[0]) == 1, "imgs[0] not paletted");
      ok(36, Imager::i_img_type($imgs[1]) == 1, "imgs[1] not paletted");
      
      # see how we go saving it
      open FH, ">testout/t105_pal.gif" or die $!;
      binmode FH;
      ok(37, i_writegif_gen(fileno(FH), { make_colors=>'addi',
					  translate=>'closest',
					  transp=>'ordered',
					}, @imgs), "write from paletted");
      close FH;
      
      # make sure nothing bad happened
      open FH, "< testout/t105_pal.gif" or die $!;
      binmode FH;
      ok(38, (my @imgs2 = Imager::i_readgif_multi(fileno(FH))) == 2,
	 "re-reading saved paletted images");
      ok(39, i_img_diff($imgs[0], $imgs2[0]) == 0, "imgs[0] mismatch");
      ok(40, i_img_diff($imgs[1], $imgs2[1]) == 0, "imgs[1] mismatch");
    }
    else {
      for (23..40) {
	print "ok $_ # skip see $buggy_giflib_file\n";
      }
    }
    # test that the OO interface warns when we supply old options
    {
      my @warns;
      local $SIG{__WARN__} = sub { push(@warns, "@_") };

      my $ooim = Imager->new;
      ok(41, $ooim->read(file=>"testout/t105.gif"), "read into object");
      ok(42, $ooim->write(file=>"testout/t105_warn.gif", interlace=>1),
        "save from object");
      ok(43, grep(/Obsolete .* interlace .* gif_interlace/, @warns),
        "check for warning");
      init(warn_obsolete=>0);
      @warns = ();
      ok(44, $ooim->write(file=>"testout/t105_warn.gif", interlace=>1),
        "save from object");
      ok(45, !grep(/Obsolete .* interlace .* gif_interlace/, @warns),
        "check for warning");
    }

    # test that we get greyscale from 1 channel images
    # we check for each makemap, and for each translate
    print "# test writes of grayscale images - ticket #365\n"; 
    my $num = 46;
    my $ooim = Imager->new(xsize=>50, ysize=>50, channels=>1);
    for (my $y = 0; $y < 50; $y += 10) {
      $ooim->box(box=>[ 0, $y, 49, $y+9], color=>NC($y*5,0,0), filled=>1);
    }
    my $ooim3 = $ooim->convert(preset=>'rgb');
    #$ooim3->write(file=>'testout/t105gray.ppm');
    my %maxerror = ( mediancut => 51000, 
                     addi => 0,
                     closest => 0,
                     perturb => 0,
                     errdiff => 0 );
    for my $makemap (qw(mediancut addi)) {
      print "# make_colors => $makemap\n";
      ok($num++, $ooim->write(file=>"testout/t105gray-$makemap.gif",
                              make_colors=>$makemap,
                              gifquant=>'gen'),
         "writing gif with makemap $makemap");
      my $im2 = Imager->new;
      if (ok($num++, $im2->read(file=>"testout/t105gray-$makemap.gif"),
             "reading written grayscale gif")) {
        my $diff = i_img_diff($ooim3->{IMG}, $im2->{IMG});
        ok($num++, $diff <= $maxerror{$makemap}, "comparing images $diff");
        #$im2->write(file=>"testout/t105gray-$makemap.ppm");
      }
      else {
        print "ok $num # skip\n";
        ++$num;
      }
    }
    for my $translate (qw(closest perturb errdiff)) {
      print "# translate => $translate\n";
      my @colors = map NC($_*50, $_*50, $_*50), 0..4;
      ok($num++, $ooim->write(file=>"testout/t105gray-$translate.gif",
                              translate=>$translate,
                              make_colors=>'none',
                              colors=>\@colors,
                              gifquant=>'gen'),
         "writing gif with translate $translate");
      my $im2 = Imager->new;
      if (ok($num++, $im2->read(file=>"testout/t105gray-$translate.gif"),
             "reading written grayscale gif")) {
        my $diff = i_img_diff($ooim3->{IMG}, $im2->{IMG});
        ok($num++, $diff <= $maxerror{$translate}, "comparing images $diff");
        #$im2->write(file=>"testout/t105gray-$translate.ppm");
      }
      else {
        print "ok $num # skip\n";
        ++$num;
      }
    }
}

sub ok ($$$) {
  my ($num, $ok, $comment) = @_;

  if ($ok) {
    print "ok $num\n";
  }
  else {
    print "not ok $num # line ",(caller)[2],": $comment \n";
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

sub _clear_tags {
  my (@imgs) = @_;

  for my $img (@imgs) {
    $img->deltag(code=>0);
  }
}

sub _add_tags {
  my ($img, %tags) = @_;

  for my $key (keys %tags) {
    Imager::i_tags_add($img, $key, 0, $tags{$key}, 0);
  }
}

sub ext_test {
  my ($testnum, $code, $count, $name) = @_;

  $count ||= 1;
  $name ||= "gif$testnum";

  # build our code
  my $script = "testout/$name.pl";
  if (open SCRIPT, "> $script") {
    print SCRIPT <<'PROLOG';
#!perl -w
if (lc $^O eq 'mswin32') {
  # avoid the dialog box that window's pops up on a GPF
  # if you want to debug this stuff, I suggest you comment out the 
  # following
  eval {
    require Win32API::File;
    Win32API::File::SetErrorMode( Win32API::File::SEM_NOGPFAULTERRORBOX());
  };
}
PROLOG

    print SCRIPT $code;
    close SCRIPT;

    my $perl = $^X;
    $perl = qq/"$perl"/ if $perl =~ / /;

    print "# script: $script\n";
    my $cmd = "$perl -Mblib $script";
    print "# command: $cmd\n";

    my $ok = 1;
    my @out = `$cmd`; # should work on DOS and Win32
    print @out;
    my $found = 0;
    for (@out) {
      if (/^not ok/) {
	$ok = 0;
	++$found;
      }
      elsif (/^ok/) {
	++$found;
      }
    }
    unless ($count == $found) {
      print "# didn't see enough ok/not ok\n";
      $ok = 0;
    }
    return $ok;
  }
  else {
    return skip($testnum, $count, "could not create test script $script: $!");
    return 0;
  }
}
