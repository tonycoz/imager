#!perl -w
use strict;

my $in = shift;
my $out = shift
  or die "Usage: $0 input.mif output.c output.h\n";

my %font;

my %glyphs;
my %chars;

open IN, "< $in"
  or die "Could not open $in: $!\n";

# load in the global font info
while (<IN>) {
  /\S/ or last;

  chomp;
  my ($key, $value) = split /:/, $_, 2;
  $font{$key} = $value;
}

for my $key (qw/size name/) {
  defined $font{$key} or die "$key missing from font block\n";
}

$font{ascender} = 0;
$font{descender} = 0;
$font{advance} = 0;
$font{height} = 0;

# work through the glyphs
while (1) {
  my %glyph;
  my $start = $.;
  while (<IN>) {
    chomp;
    /\S/ or last;
    my ($key, $value) = split /:\s*/, $_, 2;
    if ($key eq 'design') {
      $value = '';
      while (<IN>) {
	defined
	  or die "$in($.): design missing final '.'\n";
	/^\.\s*$/ and last;
	$value .= $_;
      }
      $glyph{design} = $value;
      defined or last;
    }
    else {
      $glyph{$key} = $value;
    }
  }
  
  keys %glyph
    or last;
  my $name = $glyph{name};
  defined $name or die "$in($start): no name defined\n";
  my $char = $glyph{char};
  unless (defined $char) {
    my $charcode = $glyph{charcode};
    if (defined $charcode) {
      $charcode = oct($charcode) if $charcode =~ /^0/;
      $char = chr($charcode);
    }
    else {
      length $name == 1
	or die "$in($start): no char/charcode defined for glyph with non-1 length name";
      $char = $name;
    }
  }
  my $design = $glyph{design};
  defined $design
    or die "$in($start): No design found for $name\n";
  my @design = split /\n/, $design;
  my $baseline;
  for my $i (0 .. $#design) {
    if ($design[$i] =~ /[-_]/) {
      $baseline = $i;
      $glyph{advance} = index($design[$i], '.')
	if $design[$i] =~ /\./;
      last;
    }
  }
  if (defined $baseline) {
    splice @design, $baseline, 1;
  }
  else {
    $baseline = @design;
  }
  unless (defined $glyph{offset}) {
    my $min_ws = @design ? length $design[0] : 0;
    for my $line (@design) {
      unless ($line =~ /^( +)/) {
	$min_ws = 0;
	last;
      }
      $min_ws > length $1 and $min_ws = length $1;
    }
    $glyph{offset} = $min_ws;

    if ($min_ws) {
      for my $line (@design) {
	$line = substr($line, $min_ws);
      }
    }
  }
  my $max_len = 0;
  for my $line (@design) {
    $line =~ s/ +$//;
    length $line > $max_len and $max_len = length $line;
  }
  $glyph{baseline} = $baseline;
  $glyph{width} = $max_len - $glyph{offset};
  unless (defined $glyph{advance}) {
    $glyph{advance} = $glyph{offset} + $glyph{width};
  }
  while (@design && $design[-1] !~ /\S/) {
    pop @design;
  }
  $glyph{char} = $char;
  $glyph{descender} = @design - $baseline;
  $glyph{height} = @design;
  $glyph{design} = \@design;
  for my $key (qw/descender ascender advance height/) {
    $glyph{$key} > $font{$key}
      and $font{$key} = $glyph{$key};
  }
  $glyphs{$name} = \%glyph;
}

for my $glyph (@glyphs{sort keys %glyphs}) {
  my $line_bytes = ($glyph->{width} + 7) / 8;
  my @bytes;
  print  "static unsigned char\ndesign_$glyph->{name}\[\] = \n{\n";
  for my $row (@{$glyph->{design}}) {
    print "  ";
    my $byte = 0;
    my $mask = 0x80;
    for my $bit (0 .. $glyph->{width}-1) {
      if (length $row > $bit && substr($row, $bit, 1) ne ' ') {
	$byte |= $mask;
      }
      $mask >>= 1;
      if (!$mask) {
	printf "%#x, ", $byte;
	$mask = 0x80;
	$byte = 0;
      }
    }
    if ($mask != 0x80) {
      printf "%#x, ", $byte;
    }
    print "\n";
  }
  print "};\n";
  print <<EOS;
static i_bif_glyph
glyph_$glyph->{name} = {
  $glyph->{width},
  $glyph->{height},
  $glyph->{baseline},
  $glyph->{offset},
  $glyph->{advance},
  design_$glyph->{name},
  sizeof(design_$glyph->{name}),
};

EOS
}

print "static i_bif_mapping\nmapping[] =\n{\n";
for my $glyph (@glyphs{sort { ord($glyphs{$a}{char}) <=> ord($glyphs{$b}{char}) } keys %glyphs}) {
  printf "  { %#x, glyph_%s },\n", ord($glyph->{char}), $glyph->{name};
}
print "};\n";

