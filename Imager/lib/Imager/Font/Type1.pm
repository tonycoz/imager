package Imager::Font::Type1;
use strict;
use Imager::Color;
use vars qw(@ISA);
@ISA = qw(Imager::Font);

my $t1aa;

# $T1AA is in there because for some reason (probably cache related) antialiasing
# is a system wide setting in t1 lib.

sub t1_set_aa_level {
  if (!defined $t1aa or $_[0] != $t1aa) {
    Imager::i_t1_set_aa($_[0]);
    $t1aa=$_[0];
  }
}

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
  unless ($Imager::formats{t1}) {
    $Imager::ERRSTR = "Type 1 fonts not supported in this build";
    return;
  }
  unless ($hsh{file} =~ m!^/! || $hsh{file} =~ m!^\./!) {
    $hsh{file} = './' . $hsh{file};
  }
  my $id = Imager::i_t1_new($hsh{file});
  unless ($id >= 0) { # the low-level code may miss some error handling
    $Imager::ERRSTR = "Could not load font ($id)";
    return;
  }
  return bless {
		id    => $id,
		aa    => $hsh{aa} || 0,
		file  => $hsh{file},
		type  => 't1',
		size  => $hsh{size},
		color => $hsh{color},
	       }, $class;
}

sub _draw {
  my $self = shift;
  my %input = @_;
  t1_set_aa_level($input{aa});
  if (exists $input{channel}) {
    Imager::i_t1_cp($input{image}{IMG}, $input{'x'}, $input{'y'},
		    $input{channel}, $self->{id}, $input{size},
		    $input{string}, length($input{string}), $input{align});
  } else {
    Imager::i_t1_text($input{image}{IMG}, $input{'x'}, $input{'y'}, 
		      $input{color}, $self->{id}, $input{size}, 
		      $input{string}, length($input{string}), 
		      $input{align});
  }

  return $self;
}

sub _bounding_box {
  my $self = shift;
  my %input = @_;
  return Imager::i_t1_bbox($self->{id}, $input{size}, $input{string},
			   length($input{string}));
}

1;

__END__

=head1 NAME

  Imager::Font::Type1 - low-level functions for Type1 fonts

=head1 DESCRIPTION

Imager::Font creates a Imager::Font::Type1 object when asked to create
a font object based on a .pfb file.

See Imager::Font to see how to use this type.

This class provides low-level functions that require the caller to
perform data validation

=head1 AUTHOR

Addi, Tony

=cut
