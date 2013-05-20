#!perl -w
# Make a signed or floating point version of an uncompressed TIFF
use strict;
use Getopt::Long;

my $mode = "int";
GetOptions("m|mode=s" => \$mode);

use constant TIFFTAG_BITSPERSAMPLE => 258;
use constant TIFFTAG_SAMPLEFORMAT => 339;
use constant SAMPLEFORMAT_UINT		=> 1;
use constant SAMPLEFORMAT_INT		=> 2;
use constant SAMPLEFORMAT_IEEEFP        => 3;
use constant SAMPLEFORMAT_VOID		=> 4;
use constant TIFFTAG_COMPRESSION	=> 259;
use constant COMPRESSION_NONE		=> 1;
use constant TIFFTAG_SAMPLESPERPIXEL	=> 277;

my $inname = shift;
my $outname = shift
  or die <<EOS;
Usage: $0 [-m mode] input output
  mode can be:
     int
     float
     double
EOS

open my $fh, "<", $inname
  or die "Cannot open $inname: $!\n";
binmode $fh;

my $data = do { local $/; <$fh> };

close $fh;

my $tiff = TIFFPP->new($data);

$tiff->compression == COMPRESSION_NONE
  or die "TIFF must be uncompressed\n";

my $sample_count = $tiff->samples_per_pixel;

if ($mode eq "int") {
  $tiff->each_strip
    (
     sub {
       my ($data, $tiff)  = @_;
       my ($values, $bits, $format) = $tiff->unpack_samples($data);
       my $i = 0;
       for my $value (@$values) {
	 my $limit=  1 << ($bits->[$i++] - 1);
	 $value -= $limit;
	 $i == @$bits and $i = 0;
       }
       return $tiff->pack_samples($values, undef, [ (SAMPLEFORMAT_INT) x $sample_count ]);
     }
    );
  $tiff->add_tag
    (
     tag => TIFFTAG_SAMPLEFORMAT,
     type => "SHORT",
     value => [ (SAMPLEFORMAT_INT) x $sample_count ],
    );
}
elsif ($mode eq "float") {
  $tiff->each_strip
    (
     sub {
       my ($data, $tiff)  = @_;
       my ($values, $bits, $format) = $tiff->unpack_samples($data);
       my $i = 0;
       for my $value (@$values) {
	 my $limit =  2 ** ($bits->[$i++]) - 1;
	 $value /= $limit;
	 $i == @$bits and $i = 0;
       }
       return $tiff->pack_samples($values, [ (32) x $sample_count ], [ (SAMPLEFORMAT_IEEEFP) x $sample_count ]);
     }
    );
  $tiff->add_tag
    (
     tag => TIFFTAG_SAMPLEFORMAT,
     type => "SHORT",
     value => [ (SAMPLEFORMAT_IEEEFP) x $sample_count ],
    );
  $tiff->add_tag
    (
     tag => TIFFTAG_BITSPERSAMPLE,
     type => "SHORT",
     value => [ ( 32 ) x $sample_count ]
    );
}
elsif ($mode eq "double") {
  $tiff->each_strip
    (
     sub {
       my ($data, $tiff)  = @_;
       my ($values, $bits, $format) = $tiff->unpack_samples($data);
       my $i = 0;
       for my $value (@$values) {
	 my $limit=  2 ** ($bits->[$i++] - 1) - 1;
	 $value /= $limit;
	 $i == @$bits and $i = 0;
       }
       return $tiff->pack_samples($values, [ (64) x $sample_count ], [ (SAMPLEFORMAT_IEEEFP) x $sample_count ]);
     }
    );
  $tiff->add_tag
    (
     tag => TIFFTAG_SAMPLEFORMAT,
     type => "SHORT",
     value => [ (SAMPLEFORMAT_IEEEFP) x $sample_count ],
    );
  $tiff->add_tag
    (
     tag => TIFFTAG_BITSPERSAMPLE,
     type => "SHORT",
     value => [ ( 64 ) x $sample_count ]
    );
}

$tiff->save_ifd;

open my $ofh, ">", $outname;
binmode $ofh;

print $ofh $tiff->data;
close $ofh or die;

package TIFFPP;

use constant TIFFTAG_STRIPOFFSETS => 273;
use constant TIFFTAG_STRIPBYTECOUNTS => 279;
use constant TIFFTAG_SAMPLESPERPIXEL	=> 277;
use constant TIFFTAG_BITSPERSAMPLE => 258;
use constant TIFFTAG_SAMPLEFORMAT => 339;
use constant TIFFTAG_COMPRESSION => 259;
use constant COMPRESSION_NONE		=> 1;

use constant TYPE_SHORT => 3;
use constant TYPE_LONG => 4;

use constant SAMPLEFORMAT_UINT		=> 1;
use constant SAMPLEFORMAT_INT		=> 2;
use constant SAMPLEFORMAT_IEEEFP        => 3;
use constant SAMPLEFORMAT_VOID		=> 4;

my %types;
my %type_names;

my %bit_types;

BEGIN {
  %types =
    (
     1 =>
     {
      name => "BYTE",
      size => 1,
      pack => sub { pack("C*", @{$_[0]}), scalar @{$_[0]} },
      unpack => sub { [ unpack "C*", $_[0] ] },
     },
     2 =>
     {
      name => "ASCII",
      size => 1,
     },
     3 =>
     {
      name => "SHORT",
      size => 2,
      pack => sub { pack("$_[1]{SHORT}*", @{$_[0]}), scalar @{$_[0]} },
      unpack => sub { [ unpack "$_[1]{SHORT}*", $_[0] ] },
     },
     4 =>
     {
      name => "LONG",
      size => 4,
      pack => sub { pack("$_[1]{LONG}*", @{$_[0]}), scalar @{$_[0]} },
      unpack => sub { [ unpack "$_[1]{LONG}*", $_[0] ] },
     },
     5 =>
     {
      name => "RATIONAL",
      size => 8,
      pack => sub { pack("$_[1]{LONG}*", map @$_, @{$_[0]}), scalar @{$_[0]} },
      unpack => sub {
	my @raw = unpack("$_[1]{LONG}*", $_[0]);
	return [ map [ @raw[$_*2, $_*2+1] ], 0 .. $#raw/2 ];
      },
     },
     6 =>
     {
      name => "SBYTE",
      size => 1,
      pack => sub { pack("c*", @{$_[0]}), scalar @{$_[0]} },
      unpack => sub { [ unpack "c*", $_[0] ] },
     },
     7 =>
     {
      name => "UNDEFINED",
      size => 1,
      pack => sub { $_[0], length $_[0] },
      unpack => sub { $_[0] },
     },
     8 =>
     {
      name => "SSHORT",
      size => 2,
      pack => sub { pack("$_[1]{SSHORT}*", @{$_[0]}), scalar @{$_[0]} },
      unpack => sub { [ unpack "$_[1]{SSHORT}*", $_[0] ] },
     },
     9 =>
     {
      name => "SLONG",
      size => 4,
      pack => sub { pack("$_[1]{SLONG}*", @{$_[0]}), scalar @{$_[0]} },
      unpack => sub { [ unpack "$_[1]{SLONG}*", $_[0] ] },
     },
     10 =>
     {
      name => "SRATIONAL",
      size => 8,
      pack => sub { pack("($_[1]{SLONG}$_[1]{LONG})*", map @$_, @{$_[0]}), scalar @{$_[0]} },
      unpack => sub {
	my @raw = unpack("($_[1]{SLONG}$_[1]{LONG})*", $_[0]);
	return [ map [ @raw[$_*2, $_*2+1] ], 0 .. $#raw/2 ];
      },
     },
     11 =>
     {
      name => "FLOAT",
      size => 4,
      pack => sub { pack("$_[1]{FLOAT}*", @{$_[0]}), scalar @{$_[0]} },
      unpack => sub { [ unpack "$_[1]{FLOAT}*", $_[0] ] },
     },
     12 =>
     {
      name => "DOUBLE",
      size => 8,
      pack => sub { pack("$_[1]{DOUBLE}*", @{$_[0]}), scalar @{$_[0]} },
      unpack => sub { [ unpack "$_[1]{DOUBLE}*", $_[0] ] },
     },
    );

  %type_names = map { $types{$_}->{name} => $_ } keys %types;

  %bit_types =
    (
     8 => 'BYTE',
     16 => 'SHORT',
     32 => 'LONG',
    );
}

sub new {
  my ($class, $data) = @_;

  my %opts =
    (
     data => $data,
    );

  if (substr($data, 0, 2) eq "II") {
    $opts{LONG} = "V";
    $opts{SHORT} = "v";
    $opts{SLONG} = "l<";
    $opts{SSHORT} = "s<";
    $opts{FLOAT} = "f<";
    $opts{DOUBLE} = "d<";
  }
  elsif (substr($data, 0, 2) eq "MM") {
    $opts{LONG} = "N";
    $opts{SHORT} = "n";
    $opts{SLONG} = "l>";
    $opts{SSHORT} = "s>";
    $opts{FLOAT} = "f>";
    $opts{DOUBLE} = "d>";
  }
  else {
    die "Not a TIFF file (bad byte-order)\n";
  }
  substr($data, 2, 2) eq "\x2A\0"
    or die "Not a TIFF file (bad TIFF marker)\n";
  my $ifd_off = unpack($opts{LONG}, substr($data, 4, 4));
  $ifd_off < length $data
    or die "Invalid TIFF - IFD offset too long\n";

  my $self = bless \%opts, $class;
  $self->_load_ifd(4, $ifd_off);

  $self;
}

sub data {
  $_[0]{data};
}

sub _load_ifd {
  my ($self, $off_ptr, $off) = @_;

  $self->{off_ptr} = $off_ptr;
  $self->{ifd_off} = $off;
  my $count = unpack($self->{SHORT}, substr($self->{data}, $off, 2));
  $self->{ifd_size} = $count;
  $off += 2;
  my @ifds;
  my ($short, $long) = ($self->{SHORT}, $self->{LONG});
  for my $index (1 .. $count) {
    my ($tag, $type, $count, $value) =
      unpack("$short$short${long}a4", substr($self->{data}, $off, 12));
    $types{$type}
      or die "Unknown type $type in IFD\n";
    my $size = $types{$type}{size} * $count;

    my $item_off = $size > 4 ? unpack($long, $value) : $off + 8;
    my $data = substr($self->{data}, $item_off, $size);
    push @ifds,
      {
       tag => $tag,
       type => $type,
       count => $count,
       offset => $item_off,
       data => $data,
       original => 1,
      };
    $off += 12;
  }
  my %ifd = map { $_->{tag} => $_ } @ifds;
  $self->{ifd} = \@ifds;
  $self->{ifdh} = \%ifd;

  $self->{next_ifd} = unpack($long, substr($self->{data}, $off, 4));
}

sub save_ifd {
  my ($self) = @_;

  my @ifd = sort { $a->{tag} <=> $b->{tag} } @{$self->{ifd}};
  my $ifd = pack($self->{SHORT}, scalar(@ifd));
  my ($short, $long) = ($self->{SHORT}, $self->{LONG});
  for my $entry (@ifd) {
    my %entry = %$entry;
    if (!$entry{original} && length $entry{data} > 4) {
	$entry{offset} = length $self->{data};
	$self->{data} .= $entry{data};
    }
    if (length $entry{data} > 4) {
      $ifd .= pack("$short$short$long$long", @entry{qw(tag type count offset)});
    }
    else {
      $ifd .= pack("$short$short${long}a4", @entry{qw(tag type count data)});
    }
  }
  $ifd .= pack($long, $self->{next_ifd});
  if (scalar(@ifd) <= $self->{ifd_size}) {
    substr($self->{data}, $self->{ifd_off}, length $ifd, $ifd);
  }
  else {
    $self->{ifd_off} = length $self->{data};
    $self->{data} .= $ifd;
    substr($self->{data}, $self->{off_ptr}, 4, pack($long, $self->{ifd_off}));
  }
}

sub remove_tag {
  my ($self, $tag) = @_;

  if (delete $self->{ifdh}{$tag}) {
    $self->{ifd} = [ grep $_->{tag} != $tag, @{$self->{ifd}} ];
  }
}

sub add_tag {
  my ($self, %opts) = @_;

  unless ($opts{type} =~ /[0-9]/) {
    $opts{type} = $type_names{$opts{type}}
      or die "add_tag: Invalid type\n";
  }

  if ($opts{value} && !exists $opts{data}) {
    @opts{qw(data count)} = $types{$opts{type}}{pack}->($opts{value}, $self);
  }

  if ($self->{ifdh}{$opts{tag}}) {
    $self->remove_tag($opts{tag});
  }
  push @{$self->{ifd}}, \%opts;
  $self->{ifdh}{$opts{tag}} = \%opts;
}

sub tag_value {
  my ($self, $tag) = @_;

  my $val = $self->{ifdh}{$tag}
    or return;

  return $types{$val->{type}}{unpack}->($val->{data}, $self);
}

sub each_strip {
  my ($self, $cb) = @_;

  my $offsets = $self->tag_value(TIFFTAG_STRIPOFFSETS);
  my $sizes = $self->tag_value(TIFFTAG_STRIPBYTECOUNTS);
  @$offsets == @$sizes
    or die "Strip offset and byte counts do not match\n";
  for my $i (0 .. $#$offsets) {
    my $bytes = substr($self->{data}, $offsets->[$i], $sizes->[$i]);
    $bytes = $cb->($bytes, $self);
    if (length $bytes > $sizes->[$i]) {
      $offsets->[$i] = length $self->{data};
      $self->{data} .= $bytes;
    }
    else {
      substr($self->{data}, $offsets->[$i], length $bytes, $bytes);
    }
    $sizes->[$i] = length $bytes;
  }
  my $off_type = TYPE_SHORT;
  my $count_type = TYPE_SHORT;
  $_ > 0xFFFF and $off_type = TYPE_LONG for @$offsets;
  $_ > 0xFFFF and $count_type = TYPE_LONG for @$sizes;

  $self->add_tag
    (
     tag => TIFFTAG_STRIPOFFSETS,
     type => $off_type,
     value => $offsets,
    );
  $self->add_tag
    (
     tag => TIFFTAG_STRIPBYTECOUNTS,
     type => $count_type,
     value => $sizes,
    );
}

sub _pack_format {
  my ($self, $bits, $formats) = @_;

  $bits ||= $self->_bitspersample;
  $formats ||= $self->_sampleformat;
  @$bits == @$formats
    or die "Mismatch between bitsperlsample and sampleformat counts\n";
  my $pack = '';
  for my $i (0 .. $#$bits) {
    my $type;
    if ($formats->[$i] == SAMPLEFORMAT_IEEEFP) {
      if ($bits->[$i] == 32) {
	$type = "FLOAT";
      }
      elsif ($bits->[$i] == 64) {
	$type = "DOUBLE";
      }
      else {
	die "No IEEEFP format for bits $bits->[$i]\n";
      }
    }
    else {
      $type = $bit_types{$bits->[$i]}
	or die "Can't pack $bits->[$i] bits\n";
      $type = "S$type" if $formats->[$i] == SAMPLEFORMAT_INT;
    }
    $pack .= $self->{$type};
  }

  wantarray ? ($pack, $bits, $formats) : $pack;
}

sub samples_per_pixel {
  my ($self) = @_;

  my $spp = $self->tag_value(TIFFTAG_SAMPLESPERPIXEL);
  $spp or return 1;

  return $spp->[0];
}

sub compression {
  my ($self) = @_;

  my $comp = $self->tag_value(TIFFTAG_COMPRESSION);
  $comp or return COMPRESSION_NONE;

  return $comp->[0];
}

sub _bitspersample {
  my ($self) = @_;

  my $bps = $self->tag_value(TIFFTAG_BITSPERSAMPLE);
  $bps or $bps = [ ( 1 ) x $self->samples_per_pixel ];

  return $bps;
}

sub _sampleformat {
  my ($self) = @_;

  my $formats = $self->tag_value(TIFFTAG_SAMPLEFORMAT);
  unless ($formats) {
    $formats = [ ( SAMPLEFORMAT_UINT ) x $self->samples_per_pixel ];
  }

  return $formats;
}

sub unpack_samples {
  my ($self, $data, $bits, $formats) = @_;

  my ($pack, $rbits, $rformats) = $self->_pack_format($bits, $formats);

  my $values = [ unpack "($pack)*", $data ];

  wantarray ? ( $values, $rbits, $rformats) : $values;
}

sub pack_samples {
  my ($self, $data, $bits, $formats) = @_;

  my $pack = $self->_pack_format($bits, $formats);

  pack "($pack)*", @$data;
}
