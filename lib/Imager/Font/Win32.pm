package Imager::Font::Win32;
use strict;
use vars qw(@ISA $VERSION);
@ISA = qw(Imager::Font);

$VERSION = "1.005";

# called by Imager::Font::new()
# since Win32's HFONTs include the size information this
# is just a stub
sub new {
  my ($class, %opts) = @_;

  return bless \%opts, $class;
}

sub _bounding_box {
  my ($self, %opts) = @_;
  
  my @bbox = Imager::i_wf_bbox($self->{face}, $opts{size}, $opts{string}, $opts{utf8});
}

sub _draw {
  my $self = shift;

  my %input = @_;
  if (exists $input{channel}) {
    Imager::i_wf_cp($self->{face}, $input{image}{IMG}, $input{x}, $input{'y'},
		    $input{channel}, $input{size},
		    $input{string}, $input{align}, $input{aa}, $input{utf8});
  }
  else {
    Imager::i_wf_text($self->{face}, $input{image}{IMG}, $input{x}, 
		      $input{'y'}, $input{color}, $input{size}, 
		      $input{string}, $input{align}, $input{aa}, $input{utf8});
  }
}


sub utf8 {
  return 1;
}

1;

__END__

=head1 NAME

Imager::Font::Win32 - uses Win32 GDI services for text output

=head1 SYNOPSIS

  my $font = Imager::Font->new(face=>"Arial");

=head1 DESCRIPTION

Implements font support using Win32 GDI calls.  See Imager::Font for 
usage information.

=cut
