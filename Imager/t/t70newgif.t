#!perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)


BEGIN { $| = 1; print "1..24\n"; }
END {print "not ok 1\n" unless $loaded;}

use Imager qw(:all :handy);
$loaded=1;

print "ok 1\n";

Imager::init('log'=>'testout/t70newgif.log');

$green=i_color_new(0,255,0,0);
$blue=i_color_new(0,0,255,0);

$img=Imager->new();
$img->open(file=>'testimg/scale.ppm',type=>'pnm') || print "failed: ",$img->{ERRSTR},"\n";
print "ok 2\n";


if (i_has_format("gif")) {
  $img->write(file=>'testout/t70newgif.gif',type=>'gif',gifplanes=>1,gifquant=>'lm',lmfixed=>[$green,$blue]) || print "failed: ",$img->{ERRSTR},"\nnot ";
  print "ok 3\n";

  # make sure the palette is loaded properly (minimal test)
  my $im2 = Imager->new();
  my $map;
  if ($im2->read(file=>'testimg/bandw.gif', colors=>\$map)) {
    print "ok 4\n";
    # check the palette
    if ($map) {
      print "ok 5\n";
      if (@$map == 2) {
	print "ok 6\n";
	my @sorted = sort { comp_entry($a,$b) } @$map;
	# first entry must be #000000 and second #FFFFFF
	if (comp_entry($sorted[0], NC(0,0,0)) == 0) {
	  print "ok 7\n";
	}
	else {
	  print "not ok 7 # entry should be black\n";
	}
	if (comp_entry($sorted[1], NC(255,255,255)) == 0) {
	  print "ok 8\n";
	}
	else {
	  print "not ok 8 # entry should be white\n";
	}
      } 
      else {
	print "not ok 6 # bad map size\n";
	print "ok 7 # skipped bad map size\n";
	print "ok 8 # skipped bad map size\n";
      }
    } 
    else {
      print "not ok 5 # no map returned\n";
      for (6..8) {
	print "ok $_ # skipped no map returned\n";
      }
    }
  }
  else {
    print "not ok 4 # ",$im2->errstr,"\n";
    print "ok 5 # skipped - couldn't load image\n";
  }

  # test the read_multi interface
  my @imgs = Imager->read_multi();
  @imgs and print "not ";
  print "ok 9\n";
  Imager->errstr =~ /type parameter/ or print "not ";
  print "ok 10 # ",Imager->errstr,"\n";

  @imgs = Imager->read_multi(type=>'gif');
  @imgs and print "not ";
  print "ok 11\n";
  Imager->errstr =~ /file/ or print "not ";
  print "ok 12 # ",Imager->errstr,"\n";
  # kill warning
  *NONESUCH = \20;
  @imgs = Imager->read_multi(type=>'gif', fh=>*NONESUCH);
  @imgs and print "not ";
  print "ok 13\n";
  Imager->errstr =~ /fh option not open/ or print "not ";
  print "ok 14 # ",Imager->errstr,"\n";
  @imgs = Imager->read_multi(type=>'gif', file=>'testimg/screen2.gif');
  @imgs == 2 or print "not ";
  print "ok 15\n";
  grep(!UNIVERSAL::isa($_, 'Imager'), @imgs) and print "not ";
  print "ok 16\n";
  grep($_->type eq 'direct', @imgs) and print "not ";
  print "ok 17\n";
  (my @left = $imgs[0]->tags(name=>'gif_left')) == 1 or print "not ";
  print "ok 18\n";
  my $left = $imgs[1]->tags(name=>'gif_left') or print "not ";
  print "ok 19\n";
  $left == 3 or print "not ";
  print "ok 20\n";
  if (Imager::i_giflib_version() >= 4.0) {
    open FH, "< testimg/screen2.gif" 
      or die "Cannot open testimg/screen2.gif: $!";
    binmode FH;
    my $cb = 
      sub {
        my $tmp;
        read(FH, $tmp, $_[0]) and $tmp
      };
    @imgs = Imager->read_multi(type=>'gif',
                               callback => $cb) or print "not ";
    print "ok 21\n";
    close FH;
    @imgs == 2 or print "not ";
    print "ok 22\n";

    open FH, "< testimg/screen2.gif" 
      or die "Cannot open testimg/screen2.gif: $!";
    binmode FH;
    my $data = do { local $/; <FH>; };
    close FH;
    @imgs = Imager->read_multi(type=>'gif',
			       data=>$data) or print "not ";
    print "ok 23\n";
    @imgs = 2 or print "not ";
    print "ok 24\n";
  }
  else {
    for (21..24) {
      print "ok $_ # skipped - giflib3 doesn't support callbacks\n";
    }
  }
}
else {
  for (3..24) {
    print "ok $_ # skipped: no gif support\n";
  }
}

sub comp_entry {
  my ($l, $r) = @_;
  my @l = $l->rgba;
  my @r = $r->rgba;
  return $l[0] <=> $r[0]
    || $l[1] <=> $r[1]
      || $l[2] <=> $r[2];
}
