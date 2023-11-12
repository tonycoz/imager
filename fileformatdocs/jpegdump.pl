#!perl
use strict;
use Getopt::Long;
use warnings;

my $dumpall = 0;
my $exiffile;
GetOptions(dumpall => \$dumpall,
	   "e|exif=s" => \$exiffile);

my $file = shift
  or die "Usage: $0 filename\n";

open my $fh, "<", $file
  or die "$0: cannot open '$file': $!\n";

binmode $fh;

my %codenames =
  (
    # start of frame markers, non-differential, huffman coding
    "\xFF\xC0" => "SOF0",  # baseline DCT
    "\xFF\xC1" => "SOF1",  # extended sequential DCT
    "\xFF\xC2" => "SOF2",  # progressive DCT
    "\xFF\xC3" => "SOF3",  # lossless (sequential)
    # start of frame markers, differential, huffman coding
    "\xFF\xC5" => "SOF5",  # differential sequential DCT
    "\xFF\xC6" => "SOF6",  # differential progressive DCT
    "\xFF\xC7" => "SOF7",  # differential lossless (sequential)
    # start of frame markers, non-differential, arithmetic coding
    "\xFF\xC8" => "JPG",   # Reserved for JPEG extensions
    "\xFF\xC9" => "SOF9",  # extended sequential DCT
    "\xFF\xCA" => "SOF10", # progressive DCT
    "\xFF\xCB" => "SOF11", # lossless
    # start of frame markers, differential, arithmetic coding
    "\xFF\xCD" => "SOF13", # differential sequential DCT
    "\xFF\xCF" => "SOF14", # differential progressive 
    "\xFF\xCF" => "SOF15", # differntil lossless (sequential)
    # huffman table specification
    "\xFF\xC4" => "DHT",   # define huffman tables
    # arithmetic coding conditioning specification
    "\xFF\xCC" => "DAC",   # define arithmetic coding conditioning
    # restart interval termination
    "\xFF\xD0" => "RST0",  # restart with modulo 8 count 0
    "\xFF\xD1" => "RST1",  # restart with modulo 8 count 0
    "\xFF\xD2" => "RST2",  # restart with modulo 8 count 0
    "\xFF\xD3" => "RST3",  # restart with modulo 8 count 0
    "\xFF\xD4" => "RST4",  # restart with modulo 8 count 0
    "\xFF\xD5" => "RST5",  # restart with modulo 8 count 0
    "\xFF\xD6" => "RST6",  # restart with modulo 8 count 0
    "\xFF\xD7" => "RST7",  # restart with modulo 8 count 0
    # other markers
    "\xFF\xD8" => "SOI",   # start of image
    "\xFF\xD9" => "EOI",   # end of image
    "\xFF\xDA" => "SOS",   # start of scan
    "\xFF\xDB" => "DQT",   # define quantization table
    "\xFF\xDC" => "DNL",   # define number of lines
    "\xFF\xDD" => "DRI",   # define restart interval
    "\xFF\xDE" => "DHP",   # define hierarchical progression
    "\xFF\xDF" => "EXP",   # expand reference components
    # APP0 - APP15 handled
    # \xFF\xE0 - \xFF\xEF
    "\xFF\xFE" => "COM",   # comment
   );

while (!eof($fh)) {
  my $chead;
  unless (read($fh, $chead, 2) == 2) {
    eof $fh or die "Failed to read start of chunk: $!";
    last;
  }
  # SOS will read up the next marker
 again:
  if ($chead eq "\xFF\xD8") {
    print "SOI: Start of image\n";
  }
  elsif ($chead eq "\xFF\xD9") {
    print "EOI: End of image\n";
    last;
  }
  elsif ($chead eq "\xFF\xDB") {
    # DQT
    my ($len, $dqt) = read_len_data($fh, "DQT");
    print "DQT: Define quantization tables ($len bytes)\n";
    # unclear where this table has the \xFF -> \xFF\x00 conversion used
    # to prevent markers appearing in data, which may need to be reversed
    while (length $dqt) {
      my $pq = ord($dqt) >> 4;
      my $tq = ord($dqt) & 0xF;
      substr($dqt, 0, 1, "");
      print "  Pq $pq\n";
      print "  Tq $tq\n";
      my $esize = $pq ? 2 : 1;
      my $upcode = $pq ? "S>" : "C";
      my $w = $pq ? "%5d" : "%3d";
      my $rawtbl = substr($dqt, 0, $esize * 64, "");
      while (length $rawtbl) {
        print "    ", join(" ",
                           map { sprintf $w, $_ }
                           unpack("$upcode*", substr($rawtbl, 0, $esize * 8, ""))), "\n";
      }
    }
  }
  elsif ($chead eq "\xFF\xC9") {
    # SOI 9
    my ($len, $soi9) = read_len_data($fh, "SOI9");
    print "SOI9 (arithmetic encoding) ($len bytes)\n";
  }
  elsif ($chead =~ /^\xFF[\xE0-\xEF]$/) {
    # APP0-APP15
    my ($len, $appdata) = read_len_data($fh, "APPn");
    if ($chead eq "\xFF\xE0") {
      # APP0
      my $type = substr($appdata, 0, 5, '');
	print "APP0: ($len bytes)\n  ", $type =~ tr/\0//dr, "\n";
      if ($type eq "JFIF\0") {
	my ($version, $units, $xdens, $ydens, $tx, $ty, $rest) =
	  unpack("S>CS>S>CCa*", $appdata);
	printf "    Version: %x\n", $version;
	print "    Units: $units\n";
	print "    Density: $xdens x $ydens\n";
	print "    Thumbnail: $tx x $ty\n";
      }
      else {
	# more to do
      }
    }
    elsif ($chead eq "\xFF\xE1") {
      # APP1
      if ($appdata =~ s/^Exif\0.//) {
	print "  EXIF data ($len bytes)\n";
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
  elsif ($chead eq "\xFF\xFE") {
    # COM
    my ($len, $com) = read_len_data($fh, "COM");
    print "COM: Comment ($len bytes)\n";
    print "  Text: '$com'\n";
  }
  elsif ($chead =~ /\A\xFF[\xC0-\xC3\xC5-\xC7\xC9-\xCB\xCD-\xCF]\z/) {
    # SOF0 .. SOF15 (no 4, 8, 12)
    my ($len, $sof) = read_len_data($fh, "SOF");
    my ($p, $y, $x, $nf) = unpack("CS>S>C", $sof);
    substr($sof, 0, 6, "");
    print "$codenames{$chead}: Start of frame ($len bytes)\n";
    print <<EOS;
  Sample precision: $p
  Number of lines (height): $y
  Number of samples per line (width): $x
  Number of image components (channels): $nf
EOS
    my $image_num = 0;
    while ($image_num < $nf) {
      my ($cid, $samp, $tquant) = unpack("CCC", $sof);
      my $hsamp = $samp >> 4;
      my $vsamp = $samp & 0xF;
      substr($sof, 0, 3, "");
      print <<EOS;
  Image $image_num
    Component identifier: $cid
    Horizontal sampling factor: $hsamp
    Vertical sampling factor: $vsamp
    Quantization table destination selector: $tquant
EOS
      ++$image_num;
    }
  }
  elsif ($chead eq "\xFF\xDA") {
    # SOS
    my ($len, $sos) = read_len_data($fh, "SOS");

    $chead = '';
    # SOS is special, it's terminated by the following marker, which
    # is \xFF followed by any non-zero byte
    local $/ = "\xFF";
    my $data = "";
    while (my $subdata = <$fh>) {
      if ($subdata =~ /\xFF\Z/) {
        my $byte;
        if (read($fh, $byte, 1) == 1) {
          if ($byte eq "\0") {
            # drop the NUL
            $data .= $subdata;
          }
          else {
            # new marker
            $chead = substr($subdata, -1, 1, "") . $byte;
            $data .= $subdata;
          }
        }
        else {
          # end of file?
          $data .= $subdata;
        }
      }
      else {
        # probably a truncated file, since there should be an EOI
        $data .= $subdata;
        last;
      }
    }
    print "SOS: Start of scan ($len header bytes, ", length($data), " scan bytes)\n";
    #print unpack("S*", $sos), "\n";
    #print unpack("H*", $data), "\n";

    my ($ns) = unpack("C", $sos);
    substr($sos, 0, 1, "");
    print "  Number of image components in scan: $ns\n";
    my $image_num = 0;
    
    while ($image_num < $ns) {
      my ($csj, $tda) = unpack("CC", $sos);
      substr($sos, 0, 2, "");
      my $td = $tda >> 4;
      my $ta = $tda & 0xF;
      print <<EOS;
  Component $image_num:
    Scan component selector: $csj
    DC entropy encoding table destination selector: $td
    AC entropy encoding table destination selector: $ta
EOS
      ++$image_num;
    }

    my ($ss, $se, $aboth) = unpack("CCC", $sos);
    my $ah = $aboth >> 4;
    my $al = $aboth & 0xF;
    print <<EOS;
  Start of spectral/predictor selection: $ss
  End of spectral/predictor selection: $se
  Successive approximation bit position high: $ah
  Successive approximation bit position low or point transform: $al
EOS
    print "  ", length($data), " bytes of encoded image\n";

    # we've read the marker, skip that on the next iteration
    if ($chead) {
      goto again;
    }
  }
  else {
    # not processed
    my $name = $codenames{$chead} || ("Unknown ". unpack("H*", $chead));
    my ($len, $unknown) = read_len_data($fh, $name);
    print "$name: not parsed ($len bytes)\n";
  }
}

sub read_len_data {
  my ($f, $type) = @_;

  my $clen;
  unless (read($f, $clen, 2) == 2) {
    die "Couldn't read length for $type\n";
  }
  my $len = unpack("S>", $clen);
  my $data;
  unless (read($f, $data, $len-2) == $len-2) {
    print "read only ", length $data, " bytes, expected $len\n";
    die "Couldn't read application data for $type\n";
  }
  return ($len, $data);
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
