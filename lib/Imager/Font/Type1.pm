package Imager::Font::Type1;
use strict;
use Imager::Color;
use vars qw(@ISA $VERSION);
@ISA = qw(Imager::Font);

$VERSION = "1.011";

*_first = \&Imager::Font::_first;

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

  if($hsh{afm}) {
	  unless (-e $hsh{afm}) {
	    $Imager::ERRSTR = "Afm file $hsh{afm} not found";
	    return;
	  }
	  unless ($hsh{afm} =~ m!^/! || $hsh{afm} =~ m!^\./!) {
	    $hsh{file} = './' . $hsh{file};
	  }
  } else {
	  $hsh{afm} = 0;
  }

  my $id = Imager::i_t1_new($hsh{file},$hsh{afm});
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
  my $flags = '';
  $flags .= 'u' if $input{underline};
  $flags .= 's' if $input{strikethrough};
  $flags .= 'o' if $input{overline};
  if (exists $input{channel}) {
    Imager::i_t1_cp($input{image}{IMG}, $input{'x'}, $input{'y'},
		    $input{channel}, $self->{id}, $input{size},
		    $input{string}, length($input{string}), $input{align},
                    $input{utf8}, $flags);
  } else {
    Imager::i_t1_text($input{image}{IMG}, $input{'x'}, $input{'y'}, 
		      $input{color}, $self->{id}, $input{size}, 
		      $input{string}, length($input{string}), 
		      $input{align}, $input{utf8}, $flags);
  }

  return $self;
}

sub _bounding_box {
  my $self = shift;
  my %input = @_;
  my $flags = '';
  $flags .= 'u' if $input{underline};
  $flags .= 's' if $input{strikethrough};
  $flags .= 'o' if $input{overline};
  return Imager::i_t1_bbox($self->{id}, $input{size}, $input{string},
			   length($input{string}), $input{utf8}, $flags);
}

# check if the font has the characters in the given string
sub has_chars {
  my ($self, %hsh) = @_;

  unless (defined $hsh{string} && length $hsh{string}) {
    $Imager::ERRSTR = "No string supplied to \$font->has_chars()";
    return;
  }
  return Imager::i_t1_has_chars($self->{id}, $hsh{string}, 
				_first($hsh{'utf8'}, $self->{utf8}, 0));
}

sub utf8 {
  1;
}

sub face_name {
  my ($self) = @_;

  Imager::i_t1_face_name($self->{id});
}

sub glyph_names {
  my ($self, %input) = @_;

  my $string = $input{string};
  defined $string
    or return Imager->_set_error("no string parameter passed to glyph_names");
  my $utf8 = _first($input{utf8} || 0);

  Imager::i_t1_glyph_name($self->{id}, $string, $utf8);
}


1;

__END__

=head1 NAME

  Imager::Font::Type1 - low-level functions for Type1 fonts

=head1 DESCRIPTION

Imager::Font creates a Imager::Font::Type1 object when asked to create
a font object based on a C<.pfb> file.

See Imager::Font to see how to use this type.

This class provides low-level functions that require the caller to
perform data validation

By default Imager no longer creates the F<t1lib.log> log file.  You
can re-enable that by calling Imager::init() with the C<t1log> option:

  Imager::init(t1log=>1);

This must be called before creating any fonts.

Currently specific to Imager::Font::Type1, you can use the following
flags when drawing text or calculating a bounding box:

=for stopwords overline strikethrough

=over

=item *

C<underline> - Draw the text with an underline.

=item *

C<overline> - Draw the text with an overline.

=item *

C<strikethrough> - Draw the text with a strikethrough.

=back

Obviously, if you're calculating the bounding box the size of the line
is included in the box, and the line isn't drawn :)

=head1 AUTHOR

Addi, Tony

=cut
