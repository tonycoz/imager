package Imager::Filter::DynTest;
use 5.006;
use strict;
use Imager;

BEGIN {
  our $VERSION = "0.03";
  
  require XSLoader;
  XSLoader::load('Imager::Filter::DynTest', $VERSION);
}


sub _lin_stretch {
  my %hsh = @_;

  lin_stretch($hsh{image}, $hsh{a}, $hsh{b});
}

Imager->register_filter(type=>'lin_stretch',
                        callsub => \&_lin_stretch,
                        defaults => { a => 0, b => 255 },
                        callseq => [ qw/image a b/ ]);

1;
