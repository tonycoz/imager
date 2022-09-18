#!perl
use strict;
use warnings;
use Imager;
use Imager::Test qw(to_linear_srgb to_gamma_srgb to_linear_srgbf to_gamma_srgbf);

use Test::More;

# round trip
{
  for my $level ( 0 .. 255 ) {
    my $lin = to_linear_srgb($level);
    my $gam = to_gamma_srgb($lin);

    is($gam, $level, "round trip $level -> $lin -> $gam");
  }
}

done_testing();
