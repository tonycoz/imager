package Imager::File::TIFF;
use 5.006;
use strict;
use Imager;

BEGIN {
  our $VERSION = "0.99";

  require XSLoader;
  XSLoader::load('Imager::File::TIFF', $VERSION);
}

Imager->register_reader
  (
   type=>'tiff',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;

     my $allow_incomplete = $hsh{allow_incomplete};
     defined $allow_incomplete or $allow_incomplete = 0;

     my $page = $hsh{page};
     defined $page or $page = 0;
     $im->{IMG} = i_readtiff_wiol($io, $allow_incomplete, $page);

     unless ($im->{IMG}) {
       $im->_set_error(Imager->_error_as_msg);
       return;
     }

     return $im;
   },
   multiple =>
   sub {
     my ($io, %hsh) = @_;

     my @imgs = i_readtiff_multi_wiol($io);
     unless (@imgs) {
       Imager->_set_error(Imager->_error_as_msg);
       return;
     }

     return map bless({ IMG => $_, ERRSTR => undef }, "Imager"), @imgs;
   },
  );

Imager->register_writer
  (
   type=>'tiff',
   single => 
   sub { 
     my ($im, $io, %hsh) = @_;

     $im->_set_opts(\%hsh, "i_", $im);
     $im->_set_opts(\%hsh, "tiff_", $im);
     $im->_set_opts(\%hsh, "exif_", $im);

     if (defined $hsh{class} && $hsh{class} eq "fax") {
       my $fax_fine = $hsh{fax_fine};
       defined $fax_fine or $fax_fine = 1;
       if (!i_writetiff_wiol_faxable($im->{IMG}, $io, $fax_fine)) {
	 $im->{ERRSTR} = Imager->_error_as_msg();
	 return undef;
       }
     }
     else {
       unless (i_writetiff_wiol($im->{IMG}, $io)) {
	 $im->_set_error(Imager->_error_as_msg);
	 return;
       }
     }
     return $im;
   },
   multiple =>
   sub {
     my ($class, $io, $opts, @ims) = @_;

     Imager->_set_opts($opts, "tiff_", @ims);
     Imager->_set_opts($opts, "exif_", @ims);

     my @work = map $_->{IMG}, @ims;
     my $tiff_class = $opts->{class};
     defined $tiff_class or $tiff_class = "";

     my $result;
     if ($tiff_class eq "fax") {
       my $fax_fine = $opts->{fax_fine};
       defined $fax_fine or $fax_fine = 1;
       $result = i_writetiff_multi_wiol_faxable($io, $fax_fine, @work);
     }
     else {
       $result = i_writetiff_multi_wiol($io, @work);
     }
     unless ($result) {
       $class->_set_error($class->_error_as_msg);
       return;
     }

     return 1;
   },
  );

__END__

=head1 NAME

Imager::File::TIFF - read and write TIFF files

=head1 SYNOPSIS

  use Imager;

  my $img = Imager->new;
  $img->read(file=>"foo.tiff")
    or die $img->errstr;

  $img->write(file => "foo.tif")
    or die $img->errstr;

  my @codecs = Imager::File::TIFF->codecs;

=head1 DESCRIPTION

Imager's TIFF support is documented in L<Imager::Files>.

=head1 CLASS METHODS

=over

=item Imager::File::TIFF->codecs

Returns a list of hashrefs, each hash contains:

=over

=item *

C<code> - the numeric TIFF defined identifier for this compression
codec.

=item *

C<name> - the short name traditionally used by Imager::File::TIFF for
this compression codec.  This may be an empty string if
Imager::File::TIFF doesn't have a name for this codec.

=item *

C<description> - the C<libtiff> defined name for this codec.  You can
now supply this name in the C<tiff_compression> tag to select this
compression.

=back

=item Imager::File::TIFF->builddate

The release date of the version of C<libtiff> Imager::File::TIFF was
built with.  eg. C<20230609>.

=item Imager::File::TIFF->buildversion

The version number of C<libtiff> Imager::File::TIFF was built with.
eg. C<4.5.0>.  Only available from C<libtiff> 4.5.0.

=item Imager::File::TIFF->libversion

The version number of C<libtiff> Imager::File::TIFF is running with.
This should be the same as C<buildversion> at build time, but an
upgrade to libtiff may result in C<libversion> changing without
C<buildversion> changing.

=back

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

Imager, Imager::Files.

http://www.simplesystems.org/libtiff/

=cut
