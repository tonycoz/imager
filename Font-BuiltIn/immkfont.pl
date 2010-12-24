#!perl -w
use strict;

my $in = shift
  or die "Usage: $0 name\n";

my $out_c = "$in.c";
my $out_h = "$in.h";

open CSRC, ">$out_c"
  or die "Cannot create $out_c: $!";
open HSRC, ">$out_h"
  or die "Cannot create $out_h: $!";

print HSRC <<EOS;
#ifndef \U$in\E_H_
#define \U$in\E_H_
#include "imbiff.h"

extern const i_bif_face i_face_$in;

#endif
EOS

print CSRC <<EOS;
#include "$out_h"

EOS

my @fonts;

for my $file (glob "$in*.imf") {
  my %font;
  
  my %glyphs;
  my %chars;
  
  open IN, "< $file"
    or die "Could not open $in: $!\n";
  
  # load in the global font info
  while (<IN>) {
    /\S/ or last;

    chomp;
    my ($key, $value) = split /: */, $_, 2;
    $font{$key} = $value;
  }
  
  for my $key (qw/size name/) {
    defined $font{$key} or die "$key missing from font block\n";
  }
  my $size = $font{size};
  unless ($font{name} eq $in) {
    print STDERR "Skipping $file, '$font{name}' != '$in'\n";
    next;
  }
  
  $font{ascender} = 0;
  $font{descender} = 0;
  $font{advance} = 0;
  $font{height} = 0;
  my $defname;
  
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
	    or die "$file($.): design missing final '.'\n";
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
    defined $name or die "$file($start): no name defined\n";
    my $char = $glyph{char};
    unless (defined $char) {
      my $charcode = $glyph{charcode};
      if (defined $charcode) {
	$charcode = oct($charcode) if $charcode =~ /^0/;
	$char = chr($charcode);
      }
      else {
	length $name == 1
	  or die "$file($start): no char/charcode defined for glyph with non-1 length name";
	$char = $name;
      }
    }
    my $design = $glyph{design};
    defined $design
      or die "$file($start): No design found for $name\n";
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
    for my $key (qw/descender advance height/) {
      defined $glyph{$key} or die "Bad $key in $char\n";
      $glyph{$key} > $font{$key}
	and $font{$key} = $glyph{$key};
    }
    $glyph{baseline} > $font{ascender}
      and $font{ascender} = $glyph{baseline};

    if (exists $glyph{default}) {
      $defname = $name;
    }

    $glyphs{$name} = \%glyph;
  }
  
  for my $glyph (@glyphs{sort keys %glyphs}) {
    my $line_bytes = ($glyph->{width} + 7) / 8;
    my @bytes;
    print CSRC "static unsigned char\ndesign_$glyph->{name}_$size\[\] = \n{\n";
    for my $row (@{$glyph->{design}}) {
      print CSRC "  ";
      my $byte = 0;
      my $mask = 0x80;
      for my $bit (0 .. $glyph->{width}-1) {
	if (length $row > $bit && substr($row, $bit, 1) ne ' ') {
	  $byte |= $mask;
	}
	$mask >>= 1;
	if (!$mask) {
	  printf CSRC "%#x, ", $byte;
	  $mask = 0x80;
	  $byte = 0;
	}
      }
      if ($mask != 0x80) {
	printf CSRC "%#x, ", $byte;
      }
      print CSRC "\n";
    }
    print CSRC "};\n";
    print CSRC <<EOS;
static i_bif_glyph
glyph_$glyph->{name}_$size = {
  $glyph->{width}, /* width */
  $glyph->{height}, /* height */
  $glyph->{baseline}, /* ascent */
  $glyph->{offset}, /* offset */
  $glyph->{advance}, /* advance */
  design_$glyph->{name}_$size, /* bits */
  sizeof(design_$glyph->{name}_$size),
};

EOS
  }

  print CSRC "static i_bif_mapping\nmapping_${size}[] =\n{\n";
  for my $glyph (@glyphs{sort { ord($glyphs{$a}{char}) <=> ord($glyphs{$b}{char}) } keys %glyphs}) {
    printf CSRC "  { %#x, &glyph_%s_%s },\n", ord($glyph->{char}), $glyph->{name}, $size;
  }
  my $x_width = $glyphs{x}{width};
  print CSRC "};\n\n";
  print CSRC <<EOS;
static i_bif_font
font_$size = {
  $size, /* size */
  $font{ascender}, /* global ascent */
  $font{descender}, /* global descent */
  $x_width, /* x width */
  &glyph_${defname}_$size, /* default glyph */
  mapping_$size, /* code mapping */
  sizeof(mapping_$size) / sizeof(*mapping_$size)
};

EOS
  push @fonts, $size;
}

print CSRC "static i_bif_font*\nface_fonts[] = {\n";
@fonts = sort { $a <=> $b } @fonts;
my $index = 0;
for my $size (@fonts) {
  my $comma = $index++ == $#fonts ? "" : ",";
  print CSRC "  &font_$size$comma\n";
}
print CSRC "};\n";

print CSRC <<EOS;
const i_bif_face i_face_$in = {
  "$in", /* face name */
  face_fonts, /* fonts */
  sizeof(face_fonts) / sizeof(*face_fonts)
};
EOS

close CSRC;
close HSRC;

