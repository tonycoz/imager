#!perl
use strict;
use Imager;
use Test::More;
use Imager::Test qw(extrachannel_tests);

SKIP:
{
  # basic extra channel access
  my $im1 = Imager->new(xsize => 12, ysize => 14, channels => 3, extrachannels => 5);
  ok($im1, "make image with 5 extra channels");
  my $im = $im1->masked(left => 1, top => 2, right => 11, bottom => 13);
  ok($im, "make masked image")
    or do { diag(Imager->errstr); skip "could not make masked image", 1 };
  extrachannel_tests($im);
}

done_testing();

