#!perl -w
use strict;
use Test::More tests => 8;
use Imager;

BEGIN { use_ok('Imager::Matrix2d', ':handy') }

my $id = Imager::Matrix2d->identity;

ok(almost_equal($id, [ 1, 0, 0,
                       0, 1, 0,
                       0, 0, 1 ]), "identity matrix");
my $trans = Imager::Matrix2d->translate('x'=>10, 'y'=>-11);
ok(almost_equal($trans, [ 1, 0, 10,
                          0, 1, -11,
                          0, 0, 1 ]), "translate matrix");

my $rotate = Imager::Matrix2d->rotate(degrees=>90);
ok(almost_equal($rotate, [ 0, -1, 0,
                           1, 0,  0,
                           0, 0,  1 ]), "rotate matrix");

my $shear = Imager::Matrix2d->shear('x'=>0.2, 'y'=>0.3);
ok(almost_equal($shear, [ 1,   0.2, 0,
                          0.3, 1,   0,
                          0,   0,   1 ]), "shear matrix");

my $scale = Imager::Matrix2d->scale('x'=>1.2, 'y'=>0.8);
ok(almost_equal($scale, [ 1.2, 0,   0,
                          0,   0.8, 0,
                          0,   0,   1 ]), "scale matrix");

my $trans_called;
$rotate = Imager::Matrix2d::Test->rotate(degrees=>90, x=>50);
ok($trans_called, "translate called on rotate with just x");

$trans_called = 0;
$rotate = Imager::Matrix2d::Test->rotate(degrees=>90, 'y'=>50);
ok($trans_called, "translate called on rotate with just y");

sub almost_equal {
  my ($m1, $m2) = @_;

  for my $i (0..8) {
    abs($m1->[$i] - $m2->[$i]) < 0.00001 or return undef;
  }
  return 1;
}

# this is used to ensure translate() is called correctly by rotate
package Imager::Matrix2d::Test;
use vars qw(@ISA);
BEGIN { @ISA = qw(Imager::Matrix2d); }

sub translate {
  my ($class, %opts) = @_;

  ++$trans_called;
  return $class->SUPER::translate(%opts);
}
