#!perl -w


use strict;
use Imager;
use IO::Seekable;
use Test::More;
use File::Spec;
use Imager::Test qw(is_image);

sub diag_skip_image($$);
sub diag_skip_errno($);

my $buggy_giflib_file = "buggy_giflib.txt";

my @cleanup = "t50basicoo.log";

-d "testout" or mkdir "testout";

Imager->open_log(log => "testout/t50basicoo.log");

# single image/file types
my @types = qw( jpeg png raw pnm gif tiff bmp tga );

# multiple image/file formats
my @mtypes = qw(tiff gif);

my %hsh=%Imager::formats;

print "# avaliable formats:\n";
for(keys %hsh) { print "# $_\n"; }

#print Dumper(\%hsh);

my $img = Imager->new();

my %files;
@files{@types} = ({ file => "JPEG/testimg/209_yonge.jpg"  },
		  { file => "testimg/test.png"  },
		  { file => "testimg/test.raw", xsize=>150, ysize=>150, type=>'raw', interleave => 0},
		  { file => "testimg/penguin-base.ppm"  },
		  { file => "GIF/testimg/expected.gif"  },
		  { file => "TIFF/testimg/comp8.tif" },
                  { file => "testimg/winrgb24.bmp" },
                  { file => "testimg/test.tga" }, );
my %writeopts =
  (
   gif=> { make_colors=>'webmap', translate=>'closest', gifquant=>'gen',
         gif_delay=>20 },
  );

for my $type (@types) {
  next unless $hsh{$type};
  print "# type $type\n";
  my %opts = %{$files{$type}};
  my @a = map { "$_=>${opts{$_}}" } keys %opts;

 SKIP:
  {
    print "#opening Format: $type, options: @a\n";
    ok($img->read( %opts ), "$type: reading from file")
      or diag_skip_image($img, "$type: reading base file failed");

    my %mopts = %opts;
    delete $mopts{file};

  SKIP:
    {
      my $fh;
      ok(open($fh, "<", $opts{file}), "$type: open $opts{file}")
	or diag_skip_errno("$type: Cannot open $opts{file}");

      binmode $fh;
      my $fhimg = Imager->new;
      ok($fhimg->read(fh=>$fh, %mopts), "$type: read from fh")
	or diag_skip_image($fhimg, "$type: couldn't read from fh");
      ok($fh->seek(0, SEEK_SET), "$type: seek after read")
	or diag_skip_errno("$type: couldn't seek back to start");
    SKIP:
      {
	ok($fhimg->read(fh=>$fh, %mopts, type=>$type), "$type: read from fh after seek")
	  or diag_skip_image($fhimg, "$type: failed to read after seek");
	is_image($img, $fhimg,
	   "$type: image comparison after fh read after seek");
      }
      ok($fh->seek(0, SEEK_SET), "$type: seek after read prep to read from fd")
	or diag_skip_errno("$type: couldn't seek");

    SKIP:
      {
	# read from a fd
	my $fdimg = Imager->new;
	ok($fdimg->read(fd=>fileno($fh), %mopts, type=>$type), "read from fd")
	  or diag_skip_image($fdimg, "$type: couldn't read from fd");
	is_image($img, $fdimg, "image comparistion after fd read");
      }
      ok($fh->close, "close fh after reads")
	or diag_skip_errno("$type: close after read failed");
    }

    # read from a memory buffer
    open my $dfh, "<", $opts{file}
      or die "Cannot open $opts{file}: $!";
    binmode $dfh;
    my $data = do { local $/; <$dfh> };
    close $dfh;
    my $bimg = Imager->new;

  SKIP:
    {
      ok($bimg->read(data=>$data, %mopts, type=>$type), "$type: read from buffer")
	or diag_skip_image($bimg, "$type: read from buffer failed");
      is_image($img, $bimg, "comparing buffer read image");
    }

    # read from callbacks, both with minimum and maximum reads
    my $buf = $data;
    my $seekpos = 0;
    my $reader_min =
      sub {
	my ($size, $maxread) = @_;
	my $out = substr($buf, $seekpos, $size);
	$seekpos += length $out;
	$out;
      };
    my $reader_max = 
      sub { 
	my ($size, $maxread) = @_;
	my $out = substr($buf, $seekpos, $maxread);
	$seekpos += length $out;
	$out;
      };
    my $seeker =
      sub {
	my ($offset, $whence) = @_;
	#print "io_seeker($offset, $whence)\n";
	if ($whence == SEEK_SET) {
	  $seekpos = $offset;
	}
	elsif ($whence == SEEK_CUR) {
	  $seekpos += $offset;
	}
	else { # SEEK_END
	  $seekpos = length($buf) + $offset;
	}
	#print "-> $seekpos\n";
	$seekpos;
      };
    my $cbimg = Imager->new;
  SKIP:
    {
      ok($cbimg->read(callback=>$reader_min, seekcb=>$seeker, type=>$type, %mopts),
		 "$type: read from callback min")
	or diag_skip_image("$type: read from callback min", $cbimg);
      is_image($cbimg, $img, "$type: comparing mincb image");
    }
  SKIP:
    {
      $seekpos = 0;
      ok($cbimg->read(callback=>$reader_max, seekcb=>$seeker, type=>$type, %mopts),
	 "$type: read from callback max")
	or diag_skip_image("$type: read from callback max", $cbimg);
      is_image($cbimg, $img, "$type: comparing maxcb image");
    }
  }
}

for my $type (@types) {
  next unless $hsh{$type};

  print "# write tests for $type\n";
  # test writes
  next unless $hsh{$type};
  my $file = "testout/t50out.$type";
  push @cleanup, "t50out.$type";
  my $wimg = Imager->new;

 SKIP:
  {
    ok($wimg->read(file=>"testimg/penguin-base.ppm"),
       "$type: cannot read base file")
      or diag_skip_image($wimg, "$type: reading base file");

    # first to a file
    print "# writing $type to a file\n";
    my %extraopts;
    %extraopts = %{$writeopts{$type}} if $writeopts{$type};
    ok($wimg->write(file=>$file, %extraopts),
       "writing $type to a file $file");

    my $fh_data;
  SKIP:
    {
      print "# writing $type to a FH\n";
      # to a FH
      ok(open(my $fh, "+>", $file), "$type: create FH test file")
	or diag_skip_errno("$type: Could not create $file");
      binmode $fh;
      ok($wimg->write(fh=>$fh, %extraopts, type=>$type),
	 "$type: writing to a FH")
	or diag_skip_image($wimg, "$type: write to fh");
      ok($fh->seek(0, SEEK_END),
	 "$type: seek after writing to a FH")
	or diag_skip_errno("$type: seek to end failed");
      ok(print($fh "SUFFIX\n"), "write to FH after writing $type");
      ok($fh->close, "closing FH after writing $type");

      my $ifh;
      ok(open($ifh, "< $file"), "opening data source")
	or diag_skip_errno("$type: couldn't re-open file");
      binmode $ifh;
      $fh_data = do { local $/; <$ifh> };
      close $ifh;

      # writing to a buffer
      print "# writing $type to a buffer\n";
      my $buf = '';
      ok($wimg->write(data=>\$buf, %extraopts, type=>$type),
	 "$type: writing to a buffer")
	or diag_skip_image($wimg, "$type: writing to buffer failed");
      $buf .= "SUFFIX\n";
      if (open my $wfh, ">", "testout/t50_buf.$type") {
	binmode $wfh;
	print $wfh $buf;
	close $wfh;
      }
      push @cleanup, "t50_buf.$type";
      is($fh_data, $buf, "comparing file data to buffer");
    }

  SKIP:
    {
      my $buf = '';
      my $seekpos = 0;
      my $did_close;
      my $writer = 
	sub {
	  my ($what) = @_;
	  if ($seekpos > length $buf) {
	    $buf .= "\0" x ($seekpos - length $buf);
	  }
	  substr($buf, $seekpos, length $what) = $what;
	  $seekpos += length $what;
	  $did_close = 0; # the close must be last
	  1;
	};
      my $reader_min = 
	sub { 
	  my ($size, $maxread) = @_;
	  my $out = substr($buf, $seekpos, $size);
	  $seekpos += length $out;
	  $out;
	};
      my $reader_max = 
	sub { 
	  my ($size, $maxread) = @_;
	  my $out = substr($buf, $seekpos, $maxread);
	  $seekpos += length $out;
	  $out;
	};
      use IO::Seekable;
      my $seeker =
	sub {
	  my ($offset, $whence) = @_;
	  #print "io_seeker($offset, $whence)\n";
	  if ($whence == SEEK_SET) {
	    $seekpos = $offset;
	  }
	  elsif ($whence == SEEK_CUR) {
	    $seekpos += $offset;
	  }
	  else { # SEEK_END
	    $seekpos = length($buf) + $offset;
	  }
	  #print "-> $seekpos\n";
	  $seekpos;
	};

      my $closer = sub { ++$did_close; };

      print "# writing $type via callbacks (mb=1)\n";
      ok($wimg->write(writecb=>$writer, seekcb=>$seeker, closecb=>$closer,
		      readcb=>$reader_min,
		      %extraopts, type=>$type, maxbuffer=>1),
	 "$type: writing to callback (mb=1)")
	or diag_skip_image($wimg, "$type: writing to callback failed");

      ok($did_close, "checking closecb called");
      $buf .= "SUFFIX\n";
    SKIP:
      {
	defined $fh_data
	  or skip "Couldn't read original file", 1;
	is($fh_data, $buf, "comparing callback output to file data");
      }
      print "# writing $type via callbacks (no mb)\n";
      $buf = '';
      $did_close = 0;
      $seekpos = 0;
      # we don't use the closecb here - used to make sure we don't get 
      # a warning/error on an attempt to call an undef close sub
      ok($wimg->write(writecb=>$writer, seekcb=>$seeker, readcb=>$reader_min,
		      %extraopts, type=>$type),
	 "writing $type to callback (no mb)")
	or diag_skip_image($wimg, "$type: failed writing to callback (no mb)");
      $buf .= "SUFFIX\n";
    SKIP:
      {
	defined $fh_data
	  or skip "Couldn't read original file", 1;
	is($fh_data, $buf, "comparing callback output to file data");
      }
    }
  }
}

undef($img);

# multi image/file tests
print "# multi-image write tests\n";
for my $type (@mtypes) {
  next unless $hsh{$type};
  print "# $type\n";

  my $file = "testout/t50out.$type";
  my $wimg = Imager->new;

  # if this doesn't work, we're so screwed up anyway
 SKIP:
  {
    ok($wimg->read(file=>"testout/t50out.$type"),
       "reading base file")
      or diag_skip_image($wimg, "$type-multi: reading base file failed");

    ok(my $wimg2 = $wimg->copy, "copying base image")
      or diag_skip_image($wimg, "$type-multi: cannot copy");
    ok($wimg2->flip(dir=>'h'), "flipping base image")
      or diag_skip_image($wimg, "$type-multi: cannot flip");

    my @out = ($wimg, $wimg2);
    my %extraopts;
    %extraopts = %{$writeopts{$type}} if $writeopts{$type};
    push @cleanup, "t50_multi.$type";
    ok(Imager->write_multi({ file=>"testout/t50_multi.$type", %extraopts },
			   @out),
       "$type-multi: writing multiple to a file")
      or diag_skip_image(Imager => "$type-multi: failed writing multiple to a file");

    # make sure we get the same back
    my @images = Imager->read_multi(file=>"testout/t50_multi.$type");
    if (ok(@images == @out, "$type-multi: checking read image count")) {
      for my $i (0 .. $#out) {
	is_image($out[$i], $images[$i],
		 "$type-multi: comparing image $i");
      }
    }
  }
}

done_testing();

Imager::malloc_state();

Imager->close_log;

END {
  unless ($ENV{IMAGER_KEEP_FILES}) {
    unlink map "testout/$_", @cleanup;
    rmdir "testout";
  }
}

sub diag_skip_image($$) {
  my ($img, $msg) = @_;

  diag "$msg: " . $img->errstr;
  skip $msg, 1;
}

sub diag_skip_errno($) {
  my ($msg) = @_;

  diag "$msg: $!";
  skip $msg, 1;
}
