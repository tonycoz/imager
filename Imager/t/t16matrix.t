#!perl -w

use Imager;
my $loaded;
BEGIN { $|=1; print "1..6\n"; }
END { print "not ok 1\n" unless $loaded; }
use Imager::Matrix2d ':handy';
print "ok 1\n";
$loaded = 1;

my $id = Imager::Matrix2d->identity;

almost_equal($id, [ 1, 0, 0,
                    0, 1, 0,
                    0, 0, 1 ]) or print "not ";
print "ok 2\n";
my $trans = Imager::Matrix2d->translate('x'=>10, 'y'=>-11);
almost_equal($trans, [ 1, 0, 10,
                       0, 1, -11,
                       0, 0, 1 ]) or print "not ";
print "ok 3\n";
my $rotate = Imager::Matrix2d->rotate(degrees=>90);
almost_equal($rotate, [ 0, -1, 0,
                        1, 0,  0,
                        0, 0,  1 ]) or print "not ";
print "ok 4\n";

my $shear = Imager::Matrix2d->shear('x'=>0.2, 'y'=>0.3);
almost_equal($shear, [ 1,   0.2, 0,
                       0.3, 1,   0,
                       0,   0,   1 ]) or print "not ";
print "ok 5\n";

my $scale = Imager::Matrix2d->scale('x'=>1.2, 'y'=>0.8);
almost_equal($scale, [ 1.2, 0,   0,
                       0,   0.8, 0,
                       0,   0,   1 ]) or print "not ";
print "ok 6\n";

sub almost_equal {
  my ($m1, $m2) = @_;

  for my $i (0..8) {
    abs($m1->[$i] - $m2->[$i]) < 0.00001 or return undef;
  }
  return 1;
}
