#!perl
use strict;
use Getopt::Long;

my $dumpall = 0;
my $exiffile;
GetOptions(dumpall => \$dumpall,
	   "e|exif=s" => \$exiffile);

my $file = shift
  or die "Usage: $0 filename\n";

open my $fh, "<", $file
  or die "$0: cannot open '$file': $!\n";

binmode $fh;


while (!eof($fh)) {
  my $chead;
  unless (read($fh, $chead, 2) == 2) {
    eof $fh or die "Failed to read start of chunk: $!";
    last;
  }
  if ($chead eq "\xFF\xD8") {
    print "Start of image\n";
  }
  elsif ($chead =~ /^\xFF[\xE0-\xEF]$/) {
    # APP0-APP15
    my $clen;
    unless (read($fh, $clen, 2) == 2) {
      die "Couldn't read length for APPn\n";
    }
    my $len = unpack("S>", $clen);
    my $appdata;
    unless (read($fh, $appdata, $len-2) == $len-2) {
      print "length ", length $appdata, " expected $len\n";
      die "Couldn't read application data for APPn\n";
    }
    if ($chead eq "\xFF\xE0") {
      # APP0
      my $type = substr($appdata, 0, 5, '');
	print "APP0 ", $type =~ tr/\0//dr, "\n";
      if ($type eq "JFIF\0") {
	my ($version, $units, $xdens, $ydens, $tx, $ty, $rest) =
	  unpack("S>CS>S>CCa*", $appdata);
	printf "  Version: %x\n", $version;
	print "  Units: $units\n";
	print "  Density: $xdens x $ydens\n";
	print "  Thumbnail: $tx x $ty\n";
      }
      else {
	# more to do
      }
    }
    elsif ($chead eq "\xFF\xE1") {
      # APP1
      if ($appdata =~ s/^Exif\0.//) {
	print "  EXIF data\n";
	if ($exiffile) {
	  open my $eh, ">", $exiffile
	    or die "Cannot create $exiffile: $!\n";
	  binmode $eh;
	  print $eh $appdata;
	  close $eh
	    or die "Cannot close $exiffile: $!\n";
	}
      }
    }
  }
  else {
    die "I don't know how to handle ", unpack("H*", $chead), "\n";
  }
}

=head HEAD

jpegdump.pl - dump the structure of a JPEG image file.

=head1 SYNOPSIS

  perl jpegdump.pl [-dumpall] [-exif=exifdata] filename

=head1 DESCRIPTION

Dumps the structure of a JPEG image file.

Options:

=over

=item *

C<-dumpall> - dump the entire contents of each chunk rather than just
the leading bytes.  Currently unimplemented.

=item *

C<< -exif I<filename> >> - extract the EXIF blob to a file.

=back

This is incomplete, I mostly wrote it to extract the EXIF blob, but I
expect I'll finish it at some point.

=cut
