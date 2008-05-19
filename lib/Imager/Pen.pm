package Imager::Pen;
use strict;

sub clone {
  my ($self) = @_;

  my $class = ref $self;
  return bless 
    { 
     pen => $self->{pen}->clone,
     DEPS => $self->{DEPS} 
    }, $class;
}

sub draw {
  my ($self, %opts) = @_;

  my $image = $opts{image}
    or return Imager->_set_error("No image supplied to Imager::Pen->draw");

  my $lines = $opts{lines}
    or return Imager->_set_error("No lines supplied to Imager::Pen->draw");

  return $self->{pen}->draw($image->{IMG}, @$lines);
}

1;
