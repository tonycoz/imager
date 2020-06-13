package GoodTestFont;
use 5.006;
use strict;

our $VERSION = "1.000";

# this doesn't do enough to be a font

sub new {
  my ($class, %opts) = @_;

  return bless \%opts, $class; # as long as it's true
}

1;
