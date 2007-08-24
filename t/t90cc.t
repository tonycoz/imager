#!perl -w
use strict;
use Test::More tests => 4;

use Imager;

Imager::init('log'=>'testout/t90cc.log');

my $img=Imager->new();
ok($img->open(file=>'testimg/scale.ppm'), 'load test image')
  or print "failed: ",$img->{ERRSTR},"\n";

ok(defined($img->getcolorcount(maxcolors=>10000)), 'check color count is small enough');
print "# color count: ".$img->getcolorcount()."\n";
is($img->getcolorcount(), 86, 'expected number of colors');
is($img->getcolorcount(maxcolors => 50), undef, 'check overflow handling');
