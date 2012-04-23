#!perl
use strict;
use Getopt::Long;

my $dumpall = 0;
my $image = 0;
GetOptions(dumpall => \$dumpall,
	   image => \$image);

my $file = shift
  or die "Usage: $0 filename\n";

open my $fh, "<", $file
  or die "$0: cannot open '$file': $!\n";

binmode $fh;

my $filehead;
read($fh, $filehead, 14) == 14
  or die "Could not read file header: $!\n";
my ($h_type, $h_size, $res1, $res2, $h_offset)
  = unpack("A2VvvV", $filehead);

$h_type eq "BM"
  or die "Not a BMP file - no BM signature\n";

print <<EOS;
File header:
  Type: $h_type
  Size: $h_size
  Res1: $res1
  Res2: $res2
  Offset: $h_offset
EOS

my $bmi;
read($fh, $bmi, 40) == 40
  or die "Could not read BITMAPINFO\n";

my ($i_size, $i_width, $i_height, $i_planes, $i_bits, $i_compress, $i_size_img, $i_xppm, $i_yppm, $clr_used, $clr_imp) =
  unpack("VVVvvVVVVVV", $bmi);
printf <<EOS,
Bitmapinfo:
  biSize: %d
  biWidth: %d
  biHeight: %d
  biPlanes: %d
  biBitCount: %d
  biCompression: %d
  biSizeImage: %d
  biXPelsPerMeter: %d
  biYPelsPerMeter: %d
  biClrUsed: %d
  biClrImportant: %d
EOS
  $i_size, $i_width, $i_height, $i_planes, $i_bits, $i_compress, $i_size_img, $i_xppm, $i_yppm, $clr_used, $clr_imp;

$i_size < 40
  and die "biSize too small\n";

if ($i_size > 40) {
  my $extra_head;
  read($fh, $extra_head, $i_size - 40) == $i_size - 40
    or die "Failed to read rest of header\n";
}
