package Imager::Pen::Thick;
use strict;
use Imager::Pen;
use vars qw(@ISA);

@ISA = qw (Imager::Pen);

my %end_types =
  (
   square => 0,
   block => 1,
   round => 2,
  );

sub new {
  my ($class, %opts) = @_;

  my $thickness = delete $opts{thickness};
  defined $thickness or $thickness = 1;
  my $corner = 0;
  my $front = _end_type('front', delete $opts{front});
  defined $front or return;
  my $back = _end_type('back', delete $opts{back});
  defined $back or return;
  my $self = bless {}, $class;
  my $raw;
  if ($opts{fill}) {
    $self->{pen} = 
      _new_thick_pen_fill($thickness, $opts{fill}{fill},
			  $corner, $front, $back);
    $self->{DEPS} = $opts{fill}{DEPS};
  }
  else {
    my $color = delete $opts{color};
    if ($color) {
      $color = Imager::_color($color);
    }
    else {
      $color = Imager::Color->new(0, 0, 0);
    }
    my $combine = Imager->_combine(scalar delete $opts{combine}, 1);
    $self->{pen} = 
      _new_thick_pen_color($thickness, $color, $combine,
			   $corner, $front, $back);
  }

  return $self;
}

sub _end_type {
  my ($name, $base) = @_;

  defined $base or return 0;
  $end_types{$base} and return $end_types{$base};
  $base =~ /^\d$/ and return $base;

  Imager->_set_error("Imager::Pen::Thick::new: Unknown value for $name");
  return;
}

1;
