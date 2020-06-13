package Imager::Font::Win32;
use 5.006;
use strict;
our @ISA = qw(Imager::Font::W32);

our $VERSION = "1.000";

require Imager::Font::W32;

1;

__END__

=head1 NAME

=for stopwords GDI

Imager::Font::Win32 - uses Win32 GDI services for text output

=head1 SYNOPSIS

  my $font = Imager::Font->new(face=>"Arial");

=head1 DESCRIPTION

This module is obsolete.

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=cut
