package Imager::Filter::DynTest;
use strict;
use Imager;
use vars qw($VERSION @ISA);

BEGIN {
  $VERSION = "0.01";
  
  eval {
    require XSLoader;
    XSLoader::load('Imager::Filter::DynTest', $VERSION);
    1;
  } or do {
    require DynaLoader;
    push @ISA, 'DynaLoader';
    bootstrap Imager::Filter::DynTest $VERSION;
  };
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
