BEGIN { $|=1; print "1..2\n"; }
END { print "not ok 1\n" unless $loaded; };
use Imager qw(:all);
++$loaded;
print "ok 1\n";
init_log("testout/t05error.log", 1);

# try to read an invalid pnm file
open FH, "< testimg/junk.ppm"
  or die "Cannot open testin/junk: $!";
binmode(FH);
my $IO = Imager::io_new_fd(fileno(FH));
my $im = i_readpnm_wiol($IO, -1);
if ($im) {
  print "not ok 2 # read of junk.ppm should have failed\n";
}
else {
  my @errors = Imager::i_errors();
  unless (@errors) {
    print "not ok 2 # no errors from i_errors()\n";
  }
  else {
    # each entry must be an array ref with 2 elements
    my $bad;
    for (@errors) {
      $bad = 1;
      if (ref ne 'ARRAY') {
	print "not ok 2 # element not an array ref\n";
	last;
      }
      if (@$_ != 2) {
	print "not ok 2 # elements array didn't have 2 elements\n";
	last;
      }
      $bad = 0;
    }
    unless ($bad) {
      print "ok 2\n";
      for (@errors) {
	print "# $_->[0]/$_->[1]\n";
      }
    }
  }
}
