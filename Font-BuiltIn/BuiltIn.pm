package Imager::Font::BuiltIn;
use strict;
use Imager;
use vars qw($VERSION @ISA);

BEGIN {
  $VERSION = "0.01";
  @ISA = qw(Exporter);
  
  eval {
    require XSLoader;
    XSLoader::load('Imager::Font::BuiltIn', $VERSION);
    1;
  } or do {
    require DynaLoader;
    push @ISA, 'DynaLoader';
    bootstrap Imager::Font::BuiltIn $VERSION;
  };
}

sub new {
  my ($class, %opts) = @_;

  my $face = $opts{face} || 'bitmap';

  my $f = i_bif_new($face);
  unless ($f) {
    Imager->_set_error(Imager->_error_as_msg);
    return;
  }

  $opts{font} = $f;

  bless \%opts, $class;
}

1;

