#!perl -w
BEGIN { $| = 1; print "1..15\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;

sub ok($$);
sub is($$$);
my $num = 1;

$loaded = 1;
ok(1, "loaded");

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
ok(!$im3, "returned an image on error");
ok(defined($Imager::ERRSTR), "No error message on failure");

# image synthesis
my $im4 = Imager::transform2({
	width=>300, height=>300,
	rpnexpr=>'x y cx cy distance !d y cy - x cx - atan2 !a @d 10 / @a + 3.1416 2 * % !a2 @a2 cy * 3.1416 / 1 @a2 sin 1 + 2 / hsv'});
ok($im4, "synthesis failed");

if ($im4) {
  $im4->write(type=>'pnm', file=>'testout/t56a.ppm')
    || die "Cannot write testout/t56a.ppm";
}

# image distortion
my $im5 = Imager::transform2({
	rpnexpr=>'x x 10 / sin 10 * y + getp1'
}, $im1);
ok($im5, "image distortion");
if ($im5) {
  $im5->write(type=>'pnm', file=>'testout/t56b.ppm')
    || die "Cannot write testout/t56b.ppm";
}

# image combination
$opts = {
rpnexpr=>'x h / !rat x w2 % y h2 % getp2 !pat x y getp1 @rat * @pat 1 @rat - * +'
};
my $im6 = Imager::transform2($opts,$im1,$im2);
ok($im6, "image combination");
if ($im6) {
  $im6->write(type=>'pnm', file=>'testout/t56c.ppm')
    || die "Cannot write testout/t56c.ppm";
}

# alpha
$opts = 
  {
   rpnexpr => '0 0 255 x y + w h + 2 - / 255 * rgba',
   channels => 4,
   width => 50,
   height => 50,
  };
my $im8 = Imager::transform2($opts);
ok($im8, "alpha output");
my $c = $im8->getpixel(x=>0, 'y'=>0);
is(($c->rgba)[3], 0, "zero alpha");
$c = $im8->getpixel(x=>49, 'y'=>49);
is(($c->rgba)[3], 255, "max alpha");

use Imager::Transform;

# some simple tests
print "# Imager::Transform\n";
my @funcs = Imager::Transform->list;
ok(@funcs, "funcs");

my $tran = Imager::Transform->new($funcs[0]);
ok($tran, "got tranform");
ok($tran->describe() eq Imager::Transform->describe($funcs[0]),
   "description");
# look for a function that takes inputs (at least one does)
my @needsinputs = grep Imager::Transform->new($_)->inputs, @funcs;
# make sure they're 
my @inputs = Imager::Transform->new($needsinputs[0])->inputs;
ok($inputs[0]{desc}, "input description");
# at some point I might want to test the actual transformations

# check lower level error handling
my $im7 = Imager::transform2({rpnexpr=>'x y getp2', width=>100, height=>100});
ok(!$im7, "expected failure on accessing invalid image");
print "# ", Imager->errstr, "\n";
ok(Imager->errstr =~ /not enough images/, "didn't get expected error");


sub ok ($$) {
  my ($test, $desc) = @_;

  if ($test) {
    print "ok $num # $desc\n";
  }
  else {
    print "not ok $num # $desc\n";
  }
  ++$num;
  $test;
}

sub is ($$$) {
  my ($left, $right, $desc) = @_;

  my $eq = $left == $right;
  unless (ok($eq, $desc)) {
    $left =~ s/\n/# \n/g;
    $left =~ s/([^\n\x20-\x7E])/"\\x".sprintf("%02X", ord $1)/ge;
    $right =~ s/\n/# \n/g;
    $right =~ s/([^\n\x20-\x7E])/"\\x".sprintf("%02X", ord $1)/ge;
    print "# not equal, left = '$left'\n";
    print "# right = '$right'\n";
  }
  $eq;
}
