#!perl
use strict;

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
while (my ($dlen, $data, $len, $type, $payload, $crc) = read_chunk($fh)) {
  dump_data($offset, $data);
  $offset += $dlen;

  $type =~ s/([^ -\x7f])/sprintf("\\x%02x", ord $1)/ge;
  print "  Type: $type\n";
  print "  Length: $len\n";
  if ($type eq 'IHDR') {
    my ($w, $h, $d, $ct, $comp, $filter, $inter) =
      unpack("NNCCCCC", $payload);
    print <<EOS;
  Width : $w
  Height: $h
  Depth: $d
  Colour type: $ct
  Filter: $filter
  Interpolation: $inter
EOS
    $colour_type = $ct;
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

  $type eq "IEND"
    and last;
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

  printf("%08x: ", $offset);
  if (length $data > 20) {
    print unpack("H*", substr($data, 0, 18)), "...\n";
  }
  else {
    print unpack("H*", $data), "\n";
  }
}
