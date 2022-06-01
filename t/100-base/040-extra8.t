#!perl
use strict;
use Imager;
use Test::More;
use Imager::Test qw(extrachannel_tests);

SKIP:
{
  # basic extra channel access
  my $im = Imager->new(xsize => 10, ysize => 11, channels => 3, extrachannels => 5);
  ok($im, "make image with 5 extra channels")
    or do { diag(Imager->errstr); skip "failed to make image", 1 };
  extrachannel_tests($im);
}

done_testing();
