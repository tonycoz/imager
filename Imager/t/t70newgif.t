# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)


BEGIN { $| = 1; print "1..8\n"; }
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
}
else {
  for (3..8) {
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
