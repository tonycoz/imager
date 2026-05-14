#!perl
use v5.36;
use Getopt::Long;

my $color = 0;
my $dump_data = 0;
GetOptions("c|color" => \$color,
           "d|data" => \$dump_data);

my $file = shift
  or die "Usage: $0 filename\n";

open my $fh, "<", $file
  or die "$0: cannot open '$file': $!\n";

binmode $fh;

my $buf;
read($fh, $buf, 6) or die "Cannot read header: $!";
my ($sig, $ver) = unpack("a3a3", $buf);
print <<SIG;
Signature: $sig
Version: $ver
SIG

read($fh, my $lsd, 7) or die "Cannot read LSD:$!";
my ($swidth, $sheight, $lsd_packed, $back_index, $pasp) =
    unpack("vvCCC", $lsd);
my $lsd_packeda = sprintf("0x%x", $lsd_packed);
my $gct_flag = ($lsd_packed & 0x80);
my $gct_flaga = $gct_flag ? "true" : "false";
my $cr = ($lsd_packed & 0x70) >> 4;
my $cra = 1<<($cr+1);
my $sorted = !!($lsd_packed & 0x08);
my $sorteda = $sorted ? "true" : "false";
my $gct_size = ($lsd_packed & 0x07);
my $gct_sizea = $gct_flag ? 1 << ($gct_size + 1) : "N/A";
my $paspa = $pasp ? ($pasp + 15) / 64 : "None";
print <<LSD;
Logical Screen Descriptor:
  Screen Width: $swidth
  Screen Height: $sheight
  Packed Flags: $lsd_packeda
  Global Color Table: $gct_flaga
  Color Resolution: $cra (raw $cr)
  Sorted Colors: $sorteda
  GCT Size: $gct_sizea
  Background Index: $back_index
  Pixel Aspect Ration: $paspa (raw: $pasp)
LSD

if ($gct_flag) {
    color_table($gct_sizea, "Global Color Table");
}
else {
        print "No Global Color Table\n";
}

while (1) {
    read($fh, my $type, 1) or die "Cannot read record type: $!";
    my $typec = ord($type);
    if ($typec == 0x2C) {
        read($fh, my $id, 9) or die "Cannot read image descriptor: $!";
        my ($left, $top, $width, $height, $id_packed) = unpack("vvvvC", $id);
        my $lct_flag = !!($id_packed & 0x80);
        my $interlace = !!($id_packed & 0x40);
        my $sorted_lct = !!($id_packed & 0x20);
        my $lct_size = ($id_packed & 0x07);
        print <<ID;
Image Descriptor:
  Left: $left
  Top: $top
  Width: $width
  Height: $height
  Local Color Table: $lct_flag
  Interlace: $interlace
  Sorted LCT: $sorted_lct
ID
        if ($lct_flag) {
            color_table(1 << ($lct_size+1), "Local Color Table");
        }
        read($fh, my $min_lzw, 1) or die "Cannot read LZW minimum size: $!";
        print "LZW Minimum code size: ", ord($min_lzw), "\n";
        while (1) {
            read($fh, my $data_size, 1) or die "Cannot read sub-block size: $!";
            my $size = ord($data_size);
            unless ($size) {
                print "Size: 0 (end of data)\n" if $dump_data;
                last;
            }
            print "Size: $size\n" if $dump_data;
            read($fh, my $data, $size) or die "Cannot sub-block data: $!";
            if ($dump_data) {
                while (length $data) {
                    my $row = substr($data, 0, 32, "");
                    print "  ", unpack("H*", $row), "\n";
                }
            }
        }
    }
    elsif ($typec == 0x3B) {
        print "Trailer\n";
        last;
    }
    else {
        die sprintf("Unknown block 0x%x", ord($type));
    }
}

sub color_table ($size, $name) {
    read($fh, my $colors, $size * 3) or die "Cannot read $name: $!";
    if ($color) {
        print "$name:\n";
        for my $index (0 .. $size-1) {
            print "  $index: ", unpack("H*", substr($colors, $index*3, 3)), "\n";
        }
    }
    else {
        print "Skipped $name\n";
    }
}

=head HEAD

gifdump.pl - dump the structure of a GIF image file.

=head1 SYNOPSIS

  perl gifdump.pl [-c|color] filename

=head1 DESCRIPTION

Dumps the structure of a GIF image file.

Options:

=over

=item *

C<-c>, C<-color> - Dump the contents of any local and global color
tables.

=back

=head1 BUGS

This doesn't handle any extension blocks at all yet.

=cut
