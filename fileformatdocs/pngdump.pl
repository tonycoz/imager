#!perl
use strict;
use IO::Uncompress::Inflate qw(inflate);
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

my $head;
read($fh, $head, 8) == 8
  or die "Cann't read header: $!\n";

my $offset = 0;
dump_data($offset, $head);
print "  Header\n";
$offset += length $head;
unless ($head eq "\x89PNG\x0d\x0A\cZ\x0A") {
  die "Header isn't a PNG header\n";
}

my $colour_type;
my $bits;
my $sline_len;
my $sline_left = 0;
my $row = 0;
while (my ($dlen, $data, $len, $type, $payload, $crc) = read_chunk($fh)) {
  dump_data($offset, $data);
  $offset += $dlen;
  my $calc_crc = crc($type . $payload);
  my $src_crc = unpack("N", $crc);

  $type =~ s/([^ -\x7f])/sprintf("\\x%02x", ord $1)/ge;
  print "  Type: $type\n";
  print "  Length: $len\n";
  printf "  CRC: %x (calculated: %x)\n", $src_crc, $calc_crc;
  if ($type eq 'IHDR') {
    my ($w, $h, $d, $ct, $comp, $filter, $inter) =
      unpack("NNCCCCC", $payload);
    print <<EOS;
  Width : $w
  Height: $h
  Depth: $d
  Colour type: $ct
  Filter: $filter
  Interlace: $inter
EOS
    $colour_type = $ct;
    $bits = $d;
    my $channels = $ct == 2 ? 3 : $ct == 4 ? 2 : $ct == 6 ? 4 : 1;
    my $bitspp = $channels * $d;
    $sline_len = int((($w * $bitspp) + 7) / 8);
    ++$sline_len; # filter byte
    print "  Line length: $sline_len\n";
  }
  elsif ($type eq 'sRGB') {
    print "  Rendering intent: ", ord($payload), "\n";
  }
  elsif ($type eq 'PLTE') {
    my $index = 0;
    while ($index * 3 < $len) {
      my @rgb = unpack("CCC", substr($payload, $index * 3, 3));
      print "  $index: @rgb\n";
      ++$index;
    }
  }
  elsif ($type eq 'tRNS') {
    if ($colour_type == 0) {
      my $g = unpack("n", $payload);
      printf "  Grey: %d (%x)\n", $g, $g;
    }
    elsif ($colour_type == 2) {
      my @rgb = unpack("nnn", $payload);
      printf "  RGB: %d, %d, %d (%x, %x, %x)\n", @rgb, @rgb;
    }
    elsif ($colour_type == 3) {
      my $index = 0;
      for my $alpha (unpack("C*", $payload)) {
	print "  Index: $index: $alpha\n";
	++$index;
      }
    }
    else {
      print "  Unexpected tRNS for colour type $colour_type\n";
    }
  }
  elsif ($type eq 'pHYs') {
    my ($hres, $vres, $unit) = unpack("NNC", $payload);
    my $unitname = $unit == 1 ? "metre" : "unknown";
    print <<EOS;
  hRes: $hres / $unitname
  vRes: $vres / $unitname
  Unit: $unit ($unitname)
EOS
  }
  elsif ($type eq 'tEXt') {
    my ($key, $value) = split /\0/, $payload, 2;
    print <<EOS;
  Keyword: $key
  Value: $value
EOS
    do_more_text($key, $value);
  }
  elsif ($type eq 'zTXt') {
    my ($key, $rest) = split /\0/, $payload, 2;
    my $ctype = ord $rest;
    my $ztxt = substr($rest, 1);
    my $value = do_inflate($ztxt);
    print <<EOS;
  Keyword: $key
  Value: $value
EOS
    do_more_text($key, $value);
  }
  elsif ($type eq 'tIME') {
    my @when = unpack("nCCCCC", $payload);
    printf "  Date: %d-%02d-%02d %02d:%02d:%02d\n", @when;
  }
  elsif ($type eq 'bKGD') {
    if ($colour_type == 2 || $colour_type == 6) {
      my @rgb = unpack("nnn", $payload);
      printf "  Background: rgb$bits(%d,%d,%d)\n", @rgb;
    }
    elsif ($colour_type == 0 || $colour_type == 4) {
      my $g = unpack("n", $payload);
      printf "  Background: grey$bits(%d)\n", $g;
    }
    if ($colour_type == 3) {
      my $index = unpack("C", $payload);
      printf "  Background: index(%d)\n", $index;
    }
  }
  elsif ($type eq "IDAT" && $image) {
    $sline_len
      or die "IDAT before IHDR!?";
    my $raw = do_inflate($payload);
    if ($sline_left) {
      print "  Continuing $row:\n";
      print "  ", unpack("H*", substr($raw, 0, $sline_left, "")), "\n";
      $sline_left = 0;
      ++$row;
    }
    while (length $raw >= $sline_len) {
      my $row_data = substr($raw, 0, $sline_len, "");
      my ($filter, $data) = unpack("CH*", $row_data);
      print "  Row $row, filter $filter\n";
      print "    $data\n";
      ++$row;
    }
    if (length $raw) {
      $sline_left = $sline_len - length $raw;
      my ($filter, $data) = unpack("CH*", $raw);
      print "  Row $row, filter $filter (partial)\n";
      print "    $data\n" if length $data;
    }
  }

  $type eq "IEND"
    and last;
}

sub do_more_text {
  my ($key, $text) = @_;

  if ($key eq 'Raw profile type xmp'
     && $text =~ s/^\s*xmp\s+\d+\s+//) {
    print "  XMP: ", pack("H*", join('', split ' ', $text)), "\n";
  }
}

sub read_chunk {
  my ($fh) = @_;

  my $rlen;
  read($fh, $rlen, 4)
    or die "Cannot read chunk length\n";
  my $len = unpack("N", $rlen);
  my $type;
  read($fh, $type, 4)
    or die "Cannot read chunk type\n";
  my $payload = "";
  if ($rlen) {
    read($fh, $payload, $len) == $len
      or die "Cannot read payload\n";
  }
  my $crc;
  read($fh, $crc, 4) == 4
    or die "Cannot read CRC\n";

  return ( $len + 12, $rlen . $type . $payload . $crc, $len, $type, $payload, $crc );
}

sub dump_data {
  my ($offset, $data) = @_;

  if (length $data > 28) {
    if ($dumpall) {
      for my $i (0 .. int((15 + length $data) / 16) - 1) {
	my $row = substr($data, $i * 16, 16);
	(my $clean = $row) =~ tr/ -~/./c;
	printf("%08x: %-32s %s\n", $offset, unpack("H*", $row), $clean);
      }
    }
    else {
      printf "%08x: %s...\n", $offset, unpack("H*", substr($data, 0, 26));
    }
  }
  else {
    printf "%08x: %s\n", $offset, unpack("H*", $data), "\n";
  }
}

#unsigned long crc_table[256];
my @crc_table;

#/* Flag: has the table been computed? Initially false. */
#   int crc_table_computed = 0;

#   /* Make the table for a fast CRC. */
#   void make_crc_table(void)
#   {
sub make_crc_table {
#     unsigned long c;
#     int n, k;
#
#     for (n = 0; n < 256; n++) {
  for my $n (0 .. 255) {
#       c = (unsigned long) n;
    my $c = $n;
#       for (k = 0; k < 8; k++) {
    for my $k (0 .. 7) {
#         if (c & 1)
#           c = 0xedb88320L ^ (c >> 1);
#         else
#           c = c >> 1;
      if ($c & 1) {
	$c = 0xedb88320 ^ ($c >> 1);
      }
      else {
	$c = $c >> 1;
      }
#   }
    }
#       crc_table[n] = c;
    $crc_table[$n] = $c;
#     }
  }
#     crc_table_computed = 1;
#   }
}

# /* Update a running CRC with the bytes buf[0..len-1]--the CRC
#    should be initialized to all 1's, and the transmitted value
#    is the 1's complement of the final running CRC (see the
#    crc() routine below). */

# unsigned long update_crc(unsigned long crc, unsigned char *buf,
#                          int len)
#   {
sub update_crc {
  my ($crc, $data) = @_;
#     unsigned long c = crc;
#     int n;
   
#     if (!crc_table_computed)
#       make_crc_table();
  @crc_table or make_crc_table();
#     for (n = 0; n < len; n++) {
#       c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
#     }
  for my $code (unpack("C*", $data)) {
    $crc = $crc_table[($crc ^ $code) & 0xFF] ^ ($crc >> 8);
  }
#     return c;
#   }
  return $crc;
}
   
#   /* Return the CRC of the bytes buf[0..len-1]. */
#   unsigned long crc(unsigned char *buf, int len)
#   {
#     return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
#   }

sub crc {
  my $data = shift;

  return update_crc(0xFFFFFFFF, $data) ^ 0xFFFFFFFF;
}

sub do_inflate {
  my $z = shift;
  my $out = '';
  inflate(\$z, \$out);

  return $out;
}

=head HEAD

pngdump.pl - dump the structure of a PNG image file.

=head1 SYNOPSIS

  perl pngdump.pl [-dumpall] [-image] filename

=head1 DESCRIPTION

Dumps the structure of a PNG image file, listing chunk types, length,
CRC and optionally the entire content of each chunk.

Options:

=over

=item *

C<-dumpall> - dump the entire contents of each chunk rather than just
the leading bytes.

=item *

C<-image> - decompress the image data (IDAT chunk) and break the
result into rows, listing the filter and filtered (raw) data for each
row.

=back

Several chunk types are extracted and displayed in a more readable
form.

=cut
