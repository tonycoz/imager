package Imager::Color::Float;
use 5.006;
use Imager;
use strict;
use Scalar::Util ();

our $VERSION = "1.008";

# It's just a front end to the XS creation functions.


# Parse color spec into an a set of 4 colors

sub _pspec {
  if (@_ == 1 && Scalar::Util::blessed($_[0])) {
    if ($_[0]->isa("Imager::Color::Float")) {
      return $_[0]->rgba;
    } elsif ($_[0]->isa("Imager::Color")) {
      return $_[0]->as_float->rgba;
    }
  }
  return (@_,1) if @_ == 3;
  return (@_    ) if @_ == 4;
  if ($_[0] =~ 
      /^\#?([\da-f][\da-f])([\da-f][\da-f])([\da-f][\da-f])([\da-f][\da-f])/i) {
    return (hex($1)/255,hex($2)/255,hex($3)/255,hex($4)/255);
  }
  if ($_[0] =~ /^\#?([\da-f][\da-f])([\da-f][\da-f])([\da-f][\da-f])/i) {
    return (hex($1)/255,hex($2)/255,hex($3)/255,1);
  }
  return ();
}

sub new {
  shift; # get rid of class name.
  my @arg = _pspec(@_);
  return @arg ? new_internal($arg[0],$arg[1],$arg[2],$arg[3]) : ();
}

sub set {
  my $self = shift;
  my @arg = _pspec(@_);
  return @arg ? set_internal($self, $arg[0],$arg[1],$arg[2],$arg[3]) : ();
}

sub CLONE_SKIP { 1 }

sub as_8bit {
  my ($self) = @_;

  my @out;
  for my $s ($self->rgba) {
    my $result = 0+sprintf("%.f", $s * 255);
    $result = $result < 0 ? 0 :
      $result > 255 ? 255 :
      $result;
    push @out, $result;
  }

  return Imager::Color->new(@out);
}

1;

__END__

=head1 NAME

Imager::Color::Float - Rough floating point sample color handling

=head1 SYNOPSIS

  $color = Imager::Color->new($red, $green, $blue);
  $color = Imager::Color->new($red, $green, $blue, $alpha);
  $color = Imager::Color->new("#C0C0FF"); # html color specification

  $color->set($red, $green, $blue);
  $color->set($red, $green, $blue, $alpha);
  $color->set("#C0C0FF"); # html color specification

  ($red, $green, $blue, $alpha) = $color->rgba();
  @hsv = $color->hsv(); # not implemented but proposed
  my $c8 = $color->as_8bit;

  $color->info();


=head1 DESCRIPTION

This module handles creating color objects used by Imager.  The idea
is that in the future this module will be able to handle color space
calculations as well.

A floating point Imager color consists of up to four components, each
in the range 0.0 to 1.0. Unfortunately the meaning of the components
can change depending on the type of image you're dealing with:

=over

=item *

for 3 or 4 channel images the color components are red, green, blue,
alpha.

=item *

for 1 or 2 channel images the color components are gray, alpha, with
the other two components ignored.

=back

An alpha value of zero is fully transparent, an alpha value of 1.0 is
fully opaque.

=head1 METHODS

=over 4

=item new

This creates a color object to pass to functions that need a color argument.

=item set

This changes an already defined color.  Note that this does not affect any places
where the color has been used previously.

=item rgba()

This returns the red, green, blue and alpha channels of the color the
object contains.

=item info

Calling info merely dumps the relevant color to the log.

=item red

=item green

=item blue

=item alpha

Returns the respective component as a floating point value typically
from 0 to 1.0.

=item as_8bit

Returns the color as the roughly equivalent 8-bit Imager::Color
object.  Samples below zero or above 1.0 are clipped.

=back

=head2 Setting colors

The new() and set() methods can accept the following parameters:

=over

=item *

an Imager::Color::Float object

=item *

an Imager::Color object, the ranges of samples are translated from 0...255 to 0.0...1.0.

=item *

3 values, treated as red, green, blue

=item *

4 values, treated as red, green, blue, alpha

=item *

an 8 character hex color, optionally preceded by C<#>.

=item *

a 6 character hex color, optionally preceded by C<#>.

=back

=head1 AUTHOR

Arnar M. Hrafnkelsson, addi@umich.edu
And a great deal of help from others - see the C<README> for a complete
list.

=head1 SEE ALSO

Imager(3), Imager::Color.

http://imager.perl.org/

=cut

