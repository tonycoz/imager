# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..4\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;
$loaded = 1;
print "ok 1\n";

init_log("testout/t15color.log",1);

my $c1 = Imager::Color->new(100, 150, 200, 250);
print test_col($c1, 100, 150, 200, 250) ? "ok 2\n" : "not ok 2\n";
my $c2 = Imager::Color->new(100, 150, 200);
print test_col($c2, 100, 150, 200, 255) ? "ok 3\n" : "not ok 3\n";
my $c3 = Imager::Color->new("#6496C8");
print test_col($c3, 100, 150, 200, 255) ? "ok 4\n" : "not ok 4\n";

sub test_col {
  my ($c, $r, $g, $b, $a) = @_;
  my ($cr, $cg, $cb, $ca) = $c->rgba;
  return $r == $cr && $g == $cg && $b == $cb && $a == $ca;
}


