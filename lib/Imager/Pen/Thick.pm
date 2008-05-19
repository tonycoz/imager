package Imager::Pen::Thick;
use strict;
use Imager::Pen;
use vars qw(@ISA);

@ISA = qw (Imager::Pen);

sub new {
  my ($class, %opts) = @_;

  my $thickness = delete $opts{thickness};
  defined $thickness or $thickness = 1;
  my $corner = 0;
  my $front = 0;
  my $back = 0;
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

1;
