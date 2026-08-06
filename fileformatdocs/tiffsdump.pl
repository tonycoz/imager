use v5.36;
use Carp qw(confess);

my $name = shift
    or die "Usage: $0 filename\n";

open my $fh, "<:raw", $name
    or die "Cannot open $name: $!\n";

my $total_size = -s $fh;
my $little_endian;
my $ord; # pack code for byte order

# type info
my @types =
  (
   [ 1, "-" ],
   [ 1, "BYTE", "C" ],
   [ 1, "ASCII", "a" ],
   [ 2, "SHORT", "S" ],
   [ 4, "LONG", "L" ],
   [ 8, "RATIONAL", "LL" ],
   [ 1, "SBYTE", "c" ],
   [ 1, "UNDEFINED", "C" ],
   [ 2, "SSHORT", "s" ],
   [ 4, "SLONG", "l" ],
   [ 8, "SRATIONAL", "lL" ],
   [ 4, "FLOAT", "f" ],
   [ 8, "DOUBLE", "d" ],
   );

my $ifd0_off;
{
    my $head = do_read($fh, 0, 8);
    if (substr($head, 0, 2) eq "II") {
        printf "Little-endian size %d (0x%x)\n", ($total_size) x 2;
        $little_endian = 1;
        $ord = "<";
    }
    elsif (substr($head, 0, 2) eq "MM") {
        print "Big-endian size %d (0x%#x)\n", ($total_size) x 2;
        $little_endian = 0;
        $ord = ">";
    }
    else {
        die "Invalid header\n";
    }
    my $ver = unpack("S$ord", substr($head, 2, 2));
    unless ($ver == 42) {
        die "Unknown TIFF version $ver\n";
    }
    $ifd0_off = unpack("L$ord", substr($head, 4));
}

my $ifd = load_ifd($fh, $ifd0_off)
    or die "Failed to load first IFD\n";
do {
} while ($ifd->{next_ifd} && ($ifd = load_ifd($fh, $ifd->{next_ifd})));

sub load_ifd($fh, $base_offset) {
  my $entry_count = unpack("S$ord", do_read($fh, $base_offset, 2));
  printf "IFD offset %d (0x%x) count %d (0x%x)\n",
    ($base_offset) x 2, ($entry_count) x 2;
  my $offset = $base_offset + 2;
  my @entries;
  for my $index (1 .. $entry_count) {
    my $entry_data = do_read($fh, $offset, 12);
    my ($tag, $typeid, $count) = unpack("(SSL)$ord", $entry_data);
    my $type = $typeid >= 1 && $typeid < @types ? $types[$typeid] : $types[0];
    my $dataoff;
    if ($type->[0] * $count <= 4) {
      $dataoff = $offset + 8;
    }
    else {
      $dataoff = unpack("L$ord", substr($entry_data, 8));
    }
    printf "  $index: ifdoff %d (0x%x) tag %d (0x%x) type %d (0x%x) '%s' count %d (0x%x) dataoff %d (0x%x)\n",
      ($offset) x 2, ($tag) x 2, ($typeid) x 2, $type->[1], ($count) x 2,
      ($dataoff) x 2;
    
    push @entries, { tag => $tag, type => $type, count => $count };

    $offset += 12;
  }
  my $next_ifd = unpack("L$ord", do_read($fh, $offset, 4));
  printf "Next IFD: %d (0x%x)\n", ($next_ifd) x 2;

  return
    +{
      offset => $base_offset,
      entries => \@entries,
      next_ifd => $next_ifd,
     };
}

sub do_read($fh, $offset, $size) {
    if ($offset + $size > $total_size) {
        confess "Bad file $offset/$size vs $total_size\n";
    }
    seek($fh, $offset, 0)
        or die "Cannot seek to $offset: $!\n";
    my $buf;
    read($fh, $buf, $size) == $size
        or die "Cannot read $size bytes at $offset: $!\n";

    return $buf;
}
