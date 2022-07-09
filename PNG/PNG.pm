package Imager::File::PNG;
use 5.006;
use strict;
use Imager;

BEGIN {
  our $VERSION = "0.99";

  require XSLoader;
  XSLoader::load('Imager::File::PNG', $VERSION);
}

sub read {
  my ($class, $im, $io, %hsh) = @_;

  my $flags = 0;
  $hsh{png_ignore_benign_errors}
    and $flags |= IMPNG_READ_IGNORE_BENIGN_ERRORS;
  $im->{IMG} = i_readpng_wiol($io, $flags);

  unless ($im->{IMG}) {
    $im->_set_error(Imager->_error_as_msg);
    return;
  }
  return $im;
}

Imager->register_reader
  (
   type=>'png',
   single => 
   sub {
     my ($im, $io, %hsh) = @_;
     __PACKAGE__->read($im, $io, %hsh);
   },
  );

sub write {
  my ($class, $im, $io, %hsh) = @_;

  $im->_set_opts(\%hsh, "i_", $im);
  $im->_set_opts(\%hsh, "png_", $im);

  unless (i_writepng_wiol($im->{IMG}, $io)) {
    $im->_set_error(Imager->_error_as_msg);
    return;
  }
  return $im;
}

Imager->register_writer
  (
   type=>'png',
   single =>
   sub {
     my ($im, $io, %hsh) = @_;
     return __PACKAGE__->write($im, $io, %hsh);
   },
  );

__END__

=head1 NAME

Imager::File::PNG - read and write PNG files

=head1 SYNOPSIS

  use Imager;

  my $img = Imager->new;
  $img->read(file=>"foo.png")
    or die $img->errstr;

  $img->write(file => "foo.png")
    or die $img->errstr;

=head1 DESCRIPTION

Imager's PNG support is documented in L<Imager::Files>.

=head1 METHODS

Two low level class methods are provided, most for use by
L<Imager::File::APNG>, but it may later be used for other formats that
can encapsulate PNG such as ICO.

=over

=item C<< Imager::File::PNG->read($im, $io, %hsh) >>

Read a PNG image from the supplied L<Imager::IO> object C<$io> into
the L<Imager> object C<$im>.

Returns a true value on success.

=item C<< Imager::File::PNG->write($im, $io, %hsh) >>

Write a PNG image to the supplied L<Imager::IO> object C<$io> from the
L<Imager> object C<$im>.

Returns a true value on success.

=back

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

L<Imager>, L<Imager::Files>.

=cut
