BEGIN { $| = 1; print "1..12\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;

sub ok($$$);

$loaded = 1;
print "ok 1\n";

#$Imager::DEBUG=1;

Imager::init('log'=>'testout/t58trans2.log');

my $im1 = Imager->new();
$im1->open(file=>'testimg/penguin-base.ppm', type=>'pnm')
	 || die "Cannot read image";
my $im2 = Imager->new();
$im2->open(file=>'testimg/scale.ppm',type=>'pnm')
	|| die "Cannot read testimg/scale.ppm";

# error handling
my $opts = { rpnexpr=>'x x 10 / sin 10 * y + get1' };
my $im3 = Imager::transform2($opts);
ok(2, !$im3, "returned an image on error");
ok(3, defined($Imager::ERRSTR), "No error message on failure");

# image synthesis
my $im4 = Imager::transform2({
	width=>300, height=>300,
	rpnexpr=>'x y cx cy distance !d y cy - x cx - atan2 !a @d 10 / @a + 3.1416 2 * % !a2 @a2 cy * 3.1416 / 1 @a2 sin 1 + 2 / hsv'});
ok(4, $im4, "synthesis failed");

if ($im4) {
  $im4->write(type=>'pnm', file=>'testout/t56a.ppm')
    || die "Cannot write testout/t56a.ppm";
}

# image distortion
my $im5 = Imager::transform2({
	rpnexpr=>'x x 10 / sin 10 * y + getp1'
}, $im1);
ok(5, $im5, "image distortion");
if ($im5) {
  $im5->write(type=>'pnm', file=>'testout/t56b.ppm')
    || die "Cannot write testout/t56b.ppm";
}

# image combination
$opts = {
rpnexpr=>'x h / !rat x w2 % y h2 % getp2 !pat x y getp1 @rat * @pat 1 @rat - * +'
};
my $im6 = Imager::transform2($opts,$im1,$im2);
ok(6, $im6, "image combination");
if ($im6) {
  $im6->write(type=>'pnm', file=>'testout/t56c.ppm')
    || die "Cannot write testout/t56c.ppm";
}

use Imager::Transform;

# some simple tests
my @funcs = Imager::Transform->list or print "not ";
print "ok 7\n";
my $tran = Imager::Transform->new($funcs[0]) or print "not ";
print "ok 8\n";
$tran->describe() eq Imager::Transform->describe($funcs[0]) or print "not ";
print "ok 9\n";
# look for a function that takes inputs (at least one does)
my @needsinputs = grep Imager::Transform->new($_)->inputs, @funcs;
# make sure they're 
my @inputs = Imager::Transform->new($needsinputs[0])->inputs;
$inputs[0]{desc} or print "not ";
print "ok 10\n";
# at some point I might want to test the actual transformations

# check lower level error handling
my $im7 = Imager::transform2({rpnexpr=>'x y getp2', width=>100, height=>100});
ok(11, !$im7, "expected failure on accessing invalid image");
print "# ", Imager->errstr, "\n";
ok(12, Imager->errstr =~ /not enough images/, "didn't get expected error");



sub ok ($$$) {
  my ($num, $test, $desc) = @_;

  if ($test) {
    print "ok $num\n";
  }
  else {
    print "not ok $num # $desc\n";
  }
  $test;
}

