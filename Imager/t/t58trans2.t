BEGIN { $| = 1; print "1..6\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;

$loaded = 1;
print "ok 1\n";

#$Imager::DEBUG=1;

Imager::init('log'=>'testout/t56trans2.log');

my $im1 = Imager->new();
$im1->open(file=>'testimg/penguin-base.ppm', type=>'pnm')
	 || die "Cannot read image";
my $im2 = Imager->new();
$im2->open(file=>'testimg/scale.ppm',type=>'pnm')
	|| die "Cannot read testimg/scale.ppm";

# error handling
my $opts = { rpnexpr=>'x x 10 / sin 10 * y + get1' };
my $im3 = Imager::transform2($opts);
print $im3 ? "not ok 2\n" : "ok 2\n";
print defined($Imager::ERRSTR) ? "ok 3\n" : "not ok 3\n";

# image synthesis
my $im4 = Imager::transform2({
	width=>300, height=>300,
	rpnexpr=>'x y cx cy distance !d y cy - x cx - atan2 !a @d 10 / @a + 3.1416 2 * % !a2 @a2 cy * 3.1416 / 1 @a2 sin 1 + 2 / hsv'});
print $im4 ? "ok 4\n" : "not ok 4\n";

if ($im4) {
  $im4->write(type=>'pnm', file=>'testout/t56a.ppm')
    || die "Cannot write testout/t56a.ppm";
}

# image distortion
my $im5 = Imager::transform2({
	rpnexpr=>'x x 10 / sin 10 * y + getp1'
}, $im1);
print $im5 ? "ok 5\n" : "not ok 5\n";
if ($im5) {
  $im5->write(type=>'pnm', file=>'testout/t56b.ppm')
    || die "Cannot write testout/t56b.ppm";
}

# image combination
$opts = {
rpnexpr=>'x h / !rat x w2 % y h2 % getp2 !pat x y getp1 @rat * @pat 1 @rat - * +'
};
my $im6 = Imager::transform2($opts,$im1,$im2);
print $im6 ? "ok 6\n" : "not ok 6 # $opts->{error}\n";
if ($im6) {
  $im6->write(type=>'pnm', file=>'testout/t56c.ppm')
    || die "Cannot write testout/t56c.ppm";
}
