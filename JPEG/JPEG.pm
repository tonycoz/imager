package Imager::File::JPEG;
use 5.006;
use strict;
use Imager;

BEGIN {
  our $VERSION = "0.97";

  require XSLoader;
  XSLoader::load('Imager::File::JPEG', $VERSION);
}

Imager->register_reader
  (
   type=>'jpeg',
   single =>
   sub { 
     my ($im, $io, %hsh) = @_;

     ($im->{IMG},$im->{IPTCRAW}) = i_readjpeg_wiol( $io );

     unless ($im->{IMG}) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }
     return $im;
   },
  );

Imager->register_writer
  (
   type=>'jpeg',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;

     $im->_set_opts(\%hsh, "i_", $im);
     $im->_set_opts(\%hsh, "jpeg_", $im);
     $im->_set_opts(\%hsh, "exif_", $im);

     my $quality = $hsh{jpegquality};
     defined $quality or $quality = 75;

     if ( !i_writejpeg_wiol($im->{IMG}, $io, $quality)) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }

     return $im;
   },
  );


sub has_arith_coding {
  my $cls = shift;

  return $cls->has_encode_arith_coding && $cls->has_decode_arith_coding;
}

__END__

=head1 NAME

Imager::File::JPEG - read and write JPEG files

=head1 SYNOPSIS

  use Imager;

  my $img = Imager->new;
  $img->read(file=>"foo.jpg")
    or die $img->errstr;

  $img->write(file => "foo.jpg")
    or die $img->errstr;

  my $version = Imager::File::JPEG->libjpeg_version();
  if (Imager::File::JPEG->is_turbojpeg) { ... }
  if (Imager::File::JPEG->is_mozjpeg) { ... }

  if (Imager::File::JPEG->has_arith_coding) { ... }

=head1 DESCRIPTION

Imager's JPEG support is documented in L<Imager::Files>.

Besides providing JPEG support, Imager::File::JPEG has the following
methods:

=over

=item libjpeg_version()

  Imager::File::JPEG->libjpeg_version();

Returns version information about the variety of C<libjpeg>
Imager::File::JPEG was compiled with.  This is determined at build
time.  This includes:

=over

=item *

The library type, one of C<libjpeg>, C<libjpeg-turbo> or C<mozjpeg>.

=item *

C<version> followed by the library version number.

=item *

C<api> followed by the C<libjpeg> API version.

=back

For C<libjpeg> the API and library versions are always equal.

=item is_turbojpeg()

  Imager::File::JPEG->is_turbojpeg();

Returns true if Imager::File::JPEG was built with C<libjpeg-turbo>.
Note that C<mozjpeg> is built on top of C<libjpeg-turbo> so this will
return true for C<mozjpeg>.

=item is_mozjpeg()

  Imager::File::JPEG->is_mozjpeg();

Returns true if Imager::File::JPEG was built with C<mozjpeg>.  Note
that C<mozjpeg> doesn't define its own version numbering, so
C<mozjpeg> is detected by defines that only C<mozjpeg> currently
defines.

=item has_arith_coding()

Returns true if the C<libjpeg> variant C<Imager::File::JPEG> was built
with has both encoding and decoding support for arithmetic coding.

=item has_encode_arith_coding()

Returns true if the C<libjpeg> variant C<Imager::File::JPEG> was built
with has encoding support for arithmetic coding.

=item has_decode_arith_coding()

Returns true if the C<libjpeg> variant C<Imager::File::JPEG> was built
with has decoding support for arithmetic coding.

=back

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

Imager, Imager::Files.

=cut
