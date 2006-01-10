package Imager::Filter::Mandelbrot;
use strict;
use Imager;
use vars qw($VERSION @ISA);

BEGIN {
  $VERSION = "0.01";
  
  eval {
    require XSLoader;
    XSLoader::load('Imager::Filter::Mandelbrot', $VERSION);
    1;
  } or do {
    require DynaLoader;
    push @ISA, 'DynaLoader';
    bootstrap Imager::Filter::Mandelbrot $VERSION;
  };
}

sub _mandelbrot {
  my %hsh = @_;

  mandelbrot($hsh{image}, $hsh{minx}, $hsh{miny}, $hsh{maxx}, $hsh{maxy}, $hsh{maxiter});
}

my %defaults =
  (
   minx => -2.5,
   maxx => 1.5,
   miny => -1.5,
   maxy => 1.5,
   maxiter => 256,
  );

my @callseq = qw/image minx miny maxx maxy maxiter/;

Imager->register_filter(type=>'mandelbrot',
                        callsub => \&_mandelbrot,
                        defaults => \%defaults,
                        callseq => \@callseq);

1;
