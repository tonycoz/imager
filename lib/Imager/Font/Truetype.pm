package Imager::Font::Truetype;
use strict;
use vars qw(@ISA);
@ISA = qw(Imager::Font);

sub new {
  my $class = shift;
  my %hsh=(color=>Imager::Color->new(255,0,0,0),
	   size=>15,
	   @_);

  unless ($hsh{file}) {
    $Imager::ERRSTR = "No font file specified";
    return;
  }
  unless (-e $hsh{file}) {
    $Imager::ERRSTR = "Font file $hsh{file} not found";
    return;
  }
  unless ($Imager::formats{tt}) {
    $Imager::ERRSTR = "Type 1 fonts not supported in this build";
    return;
  }
  my $id = Imager::i_tt_new($hsh{file});
  unless ($id >= 0) { # the low-level code may miss some error handling
    $Imager::ERRSTR = "Could not load font ($id)";
    return;
  }
  return bless {
		id    => $id,
		aa    => $hsh{aa} || 0,
		file  => $hsh{file},
		type  => 'tt',
		size  => $hsh{size},
		color => $hsh{color},
	       }, $class;
}

sub _draw {
  my $self = shift;
  my %input = @_;

  # note that the string length parameter is ignored and calculated in
  # XS with SvPV(), since we want the number of bytes rather than the
  # number of characters, which is what we'd get in perl for a UTF8
  # encoded string in 5.6 and later

  if ( exists $input{channel} ) {
    Imager::i_tt_cp($self->{id},$input{image}{IMG},
		    $input{'x'}, $input{'y'}, $input{channel}, $input{size},
		    $input{string}, length($input{string}),$input{aa},
                    $input{utf8}); 
  } else {
    Imager::i_tt_text($self->{id}, $input{image}{IMG}, 
		      $input{'x'}, $input{'y'}, $input{color},
		      $input{size}, $input{string}, 
		      length($input{string}), $input{aa}, $input{utf8}); 
  }
}

sub _bounding_box {
  my $self = shift;
  my %input = @_;
  return Imager::i_tt_bbox($self->{id}, $input{size},
			   $input{string}, length($input{string}),
                           $input{utf8});
}

sub utf8 { 1 }

# check if the font has the characters in the given string
sub has_chars {
  my ($self, %hsh) = @_;

  unless (defined $hsh{string} && length $hsh{string}) {
    $Imager::ERRSTR = "No string supplied to \$font->has_chars()";
    return;
  }
  return Imager::i_tt_has_chars($self->{id}, $hsh{string}, $hsh{'utf8'} || 0);
}

1;

__END__

=head1 NAME

  Imager::Font::Truetype - low-level functions for Truetype fonts

=head1 DESCRIPTION

Imager::Font creates a Imager::Font::Truetype object when asked to create
a font object based on a .ttf file.

See Imager::Font to see how to use this type.

This class provides low-level functions that require the caller to
perform data validation.

=head1 AUTHOR

Addi, Tony

=cut
