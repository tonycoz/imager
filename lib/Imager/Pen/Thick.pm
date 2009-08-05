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
   sarrowh => 3,
   parrowh => 4,
   sarrowb => 5,
   custom  => 6,
  );

my %corner_types =
  (
   cut => 0,
   round => 1,
   ptc_30 => 2,
  );

sub new {
  my ($class, %opts) = @_;

  my $thickness = delete $opts{thickness};
  defined $thickness or $thickness = 1;
  my $corner = $opts{corner};
  defined $corner or $corner = 0;
  exists $corner_types{$corner} and $corner = $corner_types{$corner};
  unless ($corner =~ /^\d+$/) {
    Imager->_set_error("Imager::Pen::Thick::new: Unknown value for corner");
    return;
  }
  my $front = _end_type('front', delete $opts{front});
  defined $front or return;
  my $back = _end_type('back', delete $opts{back});
  defined $back or return;
  my $no_custom = 0;
  bless \$no_custom, "Imager::Pen::Thick::End";
  my $front_custom = delete $opts{custom_front} || \$no_custom;
  my $back_custom = delete $opts{custom_back} || \$no_custom;
  my $self = bless {}, $class;
  my $raw;
  if ($opts{fill}) {
    $self->{pen} = 
      _new_thick_pen_fill($thickness, $opts{fill}{fill},
			  $corner, $front, $back, $front_custom, $back_custom);
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
			   $corner, $front, $back, $front_custom, $back_custom);
  }

  return $self;
}

sub _end_type {
  my ($name, $base) = @_;

  defined $base or return 0;
  exists $end_types{$base} and return $end_types{$base};
  $base =~ /^\d$/ and return $base;

  Imager->_set_error("Imager::Pen::Thick::new: Unknown value for $name");
  return;
}

1;
