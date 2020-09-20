package Imager::TrimColorList;
use strict;
use 5.006;
use Scalar::Util ();
use List::Util ();
use POSIX ();
use Imager;

our $VERSION = "1.000";

sub new {
  my $class = shift;

  my $self = $class->_new;

  for my $entry (@_) {
    $self->add($entry)
      or return;
  }

  $self;
}

sub _add_anycolor {
  my ($self, $c1, $c2) = @_;

  if ($c1->isa("Imager::Color")) {
    return $self->add_color($c1, $c2);
  }
  else {
    return $self->add_fcolor($c1, $c2);
  }
}

sub _clamp_255 {
  my $x = shift;
  if ($x < 0) {
    return 0;
  }
  elsif ($x > 255) {
    return 255;
  }
  else {
    return int($x);
  }
}

sub add {
  my ($self, $entry) = @_;

  if (ref $entry && Scalar::Util::blessed($entry)) {
    if ($entry->isa("Imager::Color") || $entry->isa("Imager::Color::Float")) {
      return $self->_add_anycolor($entry, $entry);
    }
    else {
      Imager->_set_error("bad non-array color range entry");
      return;
    }
  }
  elsif (ref $entry && Scalar::Util::reftype($entry) eq "ARRAY") {
    if (@$entry == 1) {
      if (my $c = Imager::_color($entry->[0])) {
	if ($c->isa("Imager::Color") || $c->isa("Imager::Color::Float")) {
	  return $self->_add_anycolor($c, $c);
	}
      }
      else {
	# error set by _color()
	return;
      }
    }
    elsif (@$entry == 2) {
      # first must be a color (or convertible to a color)
      if (my $c1 = Imager::_color($entry->[0])) {
	if (Scalar::Util::looks_like_number($entry->[1]) && $entry->[1] >= 0) {
	  # convert to range
	  my $f = $entry->[1];
	  if ($c1->isa("Imager::Color")) {
	    return $self->add_color(Imager::Color->new(map { _clamp_255(POSIX::ceil($_ - ( 255 * $f ))) } $c1->rgba),
				     Imager::Color->new(map { _clamp_255($_ + ( 255 * $f )) } $c1->rgba));
	  }
	  else {
	    return $self->add_fcolor(Imager::Color::Float->new(map { $_ - $f } $c1->rgba),
				     Imager::Color::Float->new(map { $_ + $f } $c1->rgba));
	  }
	}
	elsif (my $c2 = Imager::_color($entry->[1])) {
	  return $self->_add_anycolor($c1, $c2);
	}
	else {
	  # error set by _color()
	  return;
	}
      }
      else {
	return;
      }
    }
    else {
      Imager->_set_error("array entry for color range must be 1 or 2 elements");
      return;
    }
  }
  else {
    # try as a color entry
    if (my $c1 = Imager::_color($entry)) {
      return $self->_add_anycolor($c1, $c1);
    }
    else {
      return;
    }
  }
}

sub all {
  my ($self) = @_;

  my $count = $self->count;
  my @result;
  for my $i (0 .. $count-1) {
    push @result, $self->get($i);
  }

  return @result;
}

sub describe {
  my ($self) = @_;

  my $out = <<EOS;
Imager::TrimColorList->new(
EOS
  if ($self->count) {
    for my $i (0.. $self->count()-1) {
      my $entry = $self->get($i);
      if ($entry->[0]->isa("Imager::Color")) {
	$out .= sprintf("[ Imager::Color->new(%d, %d, %d), Imager::Color->new(%d, %d, %d) ],\n",
			($entry->[0]->rgba)[0 .. 2], ($entry->[1]->rgba)[0 .. 2]);
      }
      else {
	$out .= sprintf("[ Imager::Color::Float->new(%g, %g, %g), Imager::Color::Float->new(%g, %g, %g) ],\n",
			($entry->[0]->rgba)[0 .. 2], ($entry->[1]->rgba)[0 .. 2]);
      }
    }
  }
  else {
    chomp $out;
  }
    $out .= ")\n";

  return $out;
}

sub clone {
  my ($self) = @_;

  return Imager::TrimColorList->new($self->all);
}

sub auto {
  my ($self, %hsh) = @_;

  my $name = delete $hsh{name} || "auto";
  my $auto = delete $hsh{auto} || "1";
  my $image = delete $hsh{image};
  my $tolerance = delete $hsh{tolerance};

  defined $tolerance or $tolerance = 0.01;

  unless ($image && $image->{IMG}) {
    Imager->_set_error("$name: no image supplied");
    return;
  }

  if ($auto eq "1") {
    $auto = "centre";
  }
  if ($auto eq "center" || $auto eq "centre") {
    my ($w, $h) = ( $image->getwidth(), $image->getheight() );
    return Imager::TrimColorList->new
      (
       [ $image->getpixel(x => $w / 2, y => 0     ), $tolerance ],
       [ $image->getpixel(x => $w / 2, y => $h - 1), $tolerance ],
       [ $image->getpixel(x => 0,      y => $h / 2), $tolerance ],
       [ $image->getpixel(x => $w - 1, y => $h / 2), $tolerance ],
      );
  }
  else {
    Imager->_set_error("$name: auto must be '1' or 'center'");
    return;
  }
}

1;

=head1 NAME

Imager::TrimColorList - represent a list of color ranges for Imager's trim() method.

=head1 SYNOPSIS

  use Imager::TrimColorList;

  # empty list
  my $tcl = Imager::TrimColorList->new;

  # add an entry in a variety of forms
  $tcl->add($entry);

  # add an explicit color object entry
  $tcl->add_color($c1, $c2);

  # add an explicit floating color object entry
  $tcl->add_fcolor($cf1, $cf2);

  # number of entries
  my $count = $tcl->count;

  # fetch an entry
  my $entry = $tcl->get($index);

  # fetch all entries
  my @all = $tcl->all;

  # make a list and populate it
  my $tcl = Imager::TrimColorList->new($entry1, $entry2, ...);

  # dump contents of the list as a string
  print $tcl->describe;

=head1 DESCRIPTION

An Imager::TrimColorList represents a list of color ranges to supply
to the trim() method.

Each range can be either an 8-bit color range, ie. L<Imager::Color>
objects, or a floating point color range, ie. L<Imager::Color::Float>
objects, these can be mixed freely in a single list but each range
must be 8-bit or floating point.

You can supply an entry in a small variety of forms:

=over

=item *

a simple color object of either type, or something convertible to a
color object such as a color name such as C<"red">, a hex color such
as C<"#FFF">.  Any of the forms that Imager::Color supports should
work here I<except> for the array form.  This becomes a range of only
that color.

  $tcl->add("#000");
  $tcl->add(Imager::Color->new(0, 0, 0));
  $tcl->add(Imager::Color::Float->new(0, 0, 0));

=item *

an arrayref containing a single color object, or something convertible
to a color object.  This becomes a range of only that color.

  $tcl->add([ "#000" ]);
  $tcl->add([ Imager::Color->new(0, 0, 0) ]);
  $tcl->add([ Imager::Color::Float->new(0, 0, 0) ]);

=item *

an arrayref containing two color objects of the same type, ie. both
Imager::Color objects or convertible to Imager::Color objects, or two
Imager::Color::Float objects.  This becomes a range between those two
colors inclusive.

  $tcl->add([ "#000", "#002" ]);
  $tcl->add([ Imager::Color->new(0, 0, 0), Imager::Color->new(0, 0, 2) ]);
  $tcl->add([ Imager::Color::Float->new(0, 0, 0), Imager::Color::Float->new(0, 0, 2/255) ]);

=item *

an arrayref containing a color object of either type and a number
representing the variance within the color space on either side of the
specified color to include.

  $tcl->add([ "#000", 0.01 ])
  $tcl->add([ Imager::Color->new(0, 0, 0), 0.01 ]);
  $tcl->add([ Imager::Color::Float->new(0, 0, 0), 0.01 ]);

A range specified this way with an 8-bit color clips at the top and
bottom of the sample ranges, so the example 8-bit range above goes
from (0, 0, 0) to (2, 2, 2) inclusive, while the floating point range
isn't clipped and results in the floating color range (-0.01, -0.01,
-0.01) to (0.01, 0.01, 0.01) inclusive.

=back

=head1 METHODS

=over

=item new()

=item new($entry1, ...)

Class method.  Create a new Imager::TrimColorList object and
optionally add some color ranges to it.

Returns an optionally populated Imager::TrimColorList object, or an
empty list (or undef) or failure.

=item add($entry)

Add a single range entry.  Note that this accepts a single value which
can be a color or convertible to a color, or a reference to an array
as described above.

Returns a true value on success and a false value on failure.

=item add_color($color1, $color2)

Add a single 8-bit color range going from the C<$color1> object to the
C<$color2> object inclusive.  Both parameters must be Image::Color
objects or an exception is thrown.

=item add_fcolor($fcolor1, $fcolor2)

Add a single floating point color range going from the C<$fcolor1>
object to the C<$fcolor2> object inclusive.  Both parameters must be
Image::Color::Float objects or an exception is thrown.

=item count()

Fetch the number of color ranges stored in the object.

=item get($index)

Fetch the color range at the given index.  This returns a reference to
an array containing either two Imager::Color objects or two
Imager::Color::Float objects.

Returns undef if C<$index> is out of range and does not set C<<
Imager->errstr >>.

=item all()

Fetch all ranges from the object.

=item describe()

Return a string describing the color range as code that can create the
object.

=item clone()

Duplicate the object.

=item auto()

Automatically produce a trim color list based on an input image.

This is used to implement 'auto' for image trim() and trim_rect()
methods.

Parameters:

=over

=item *

C<image> - the image to base the color list on.  Required.

=item *

C<auto> - the mechanism used to produce the color list, one of:

=over

=item *

C<1> - a "best" mechanism is selected, this is currently the C<center>
method, but it subject to change.

=item *

C<center>, C<centre> - the pixels at the center of each side of the
image are used.

=back

Default: C<1>.

=item *

C<tolerance> - used to control the range of pixel colors to be
accepted as part of the color list.  Default: 0.01.

=item *

C<name> - used internally to attribute errors back to the original
method.  Default: C<auto>.

=back

=back

If any method returns an error you can fetch a diagnostic from C<<
Imager->errstr >>.

=head1 THREADS

Imager::TrimColorList objects are properly duplicated when new perl
threads are created.

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=head1 HISTORY

Originally the range handling for this was going to be embedded in the
trim() method, but this meant that every called that used color ranges
would pay some cost as the range list was checked for names (vs color
objects) and non-standard forms such as single colors and the color
plus variance.

The object allows a simple test for the trim() C<colors> parameter
that doesn't pay that cost, and still allows a caller to use the
explicit convention.

=cut
