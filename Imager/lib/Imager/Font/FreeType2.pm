package Imager::Font::FreeType2;
use strict;
use Imager::Color;
use vars qw(@ISA);
@ISA = qw(Imager::Font);

*_first = \&Imager::Font::_first;

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
  unless ($Imager::formats{ft2}) {
    $Imager::ERRSTR = "Freetype2 not supported in this build";
    return;
  }
  my $id = i_ft2_new($hsh{file}, $hsh{'index'} || 0);
  unless ($id) { # the low-level code may miss some error handling
    $Imager::ERRSTR = Imager::_error_as_msg();
    return;
  }
  return bless {
		id       => $id,
		aa       => $hsh{aa} || 0,
		file     => $hsh{file},
		type     => 't1',
		size     => $hsh{size},
		color    => $hsh{color},
                utf8     => $hsh{utf8},
                vlayout  => $hsh{vlayout},
	       }, $class;
}

sub _draw {
  my $self = shift;
  my %input = @_;
  if (exists $input{channel}) {
    i_ft2_cp($self->{id}, $input{image}{IMG}, $input{'x'}, $input{'y'},
             $input{channel}, $input{size}, $input{sizew} || 0,
             $input{string}, , $input{align}, $input{aa}, $input{vlayout},
             $input{utf8});
  } else {
    i_ft2_text($self->{id}, $input{image}{IMG},
               $input{'x'}, $input{'y'},
               $input{color}, $input{size}, $input{sizew} || 0,
               $input{string}, $input{align}, $input{aa}, $input{vlayout},
               $input{utf8});
  }
}

sub _bounding_box {
  my $self = shift;
  my %input = @_;

  return i_ft2_bbox($self->{id}, $input{size}, $input{sizew}, $input{string}, 
		    $input{utf8});
}

sub dpi {
  my $self = shift;
  my @old = i_ft2_getdpi($self->{id});
  if (@_) {
    my %hsh = @_;
    my $result;
    unless ($hsh{xdpi} && $hsh{ydpi}) {
      if ($hsh{dpi}) {
        $hsh{xdpi} = $hsh{ydpi} = $hsh{dpi};
      }
      else {
        $Imager::ERRSTR = "dpi method requires xdpi and ydpi or just dpi";
        return;
      }
      i_ft2_setdpi($self->{id}, $hsh{xdpi}, $hsh{ydpi}) or return;
    }
  }
  
  return @old;
}

sub hinting {
  my ($self, %opts) = @_;

  i_ft2_sethinting($self->{id}, $opts{hinting} || 0);
}

sub _transform {
  my $self = shift;

  my %hsh = @_;
  my $matrix = $hsh{matrix} or return undef;

  return i_ft2_settransform($self->{id}, $matrix)
}

sub utf8 {
  return 1;
}

# check if the font has the characters in the given string
sub has_chars {
  my ($self, %hsh) = @_;

  unless (defined $hsh{string} && length $hsh{string}) {
    $Imager::ERRSTR = "No string supplied to \$font->has_chars()";
    return;
  }
  return i_ft2_has_chars($self->{id}, $hsh{string}, $hsh{'utf8'} || 0);
}

sub face_name {
  my ($self) = @_;

  i_ft2_face_name($self->{id});
}

sub can_glyph_names {
  i_ft2_can_do_glyph_names();
}

sub glyph_names {
  my ($self, %input) = @_;

  my $string = $input{string};
  defined $string
    or return Imager->_set_error("no string parameter passed to glyph_names");
  my $utf8 = _first($input{utf8}, 0);
  my $reliable_only = _first($input{reliable_only}, 1);

  my @names = i_ft2_glyph_name($self->{id}, $string, $utf8, $reliable_only);
  @names or return Imager->_set_error(Imager->_error_as_msg);

  return @names if wantarray;
  return pop @names;
}

1;

__END__

=head1 NAME

  Imager::Font::FreeType2 - low-level functions for FreeType2 text output

=head1 DESCRIPTION

Imager::Font creates a Imager::Font::FreeType2 object when asked to.

See Imager::Font to see how to use this type.

This class provides low-level functions that require the caller to
perform data validation.

This driver supports:

=over

=item transform()

=item dpi()

=item draw()

The following parameters:

=over

=item utf8

=item vlayour

=item sizew

=back

=back

=head2 Special behaviors

If you call transform() to set a transformation matrix, hinting will
be switched off.  This prevents sudden jumps in the size of the text
caused by the hinting when the transformation is the identity matrix.
If for some reason you want hinting enabled, use
$font->hinting(hinting=>1) to re-enable hinting.  This will need to be
called after I<each> call to transform().

=head1 AUTHOR

Addi, Tony

=cut
