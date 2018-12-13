#!perl -w
use strict;
use Test::More;
use ExtUtils::Manifest qw(maniread);
use File::Temp;
eval "use Pod::Spell 1.15";
plan skip_all => "Pod::Spell 1.15 required for spellchecking POD" if $@;
my @pod = @ARGV;
unless (@pod) {
  my $manifest = maniread();
  @pod = sort grep !/^inc/ && /\.(pm|pl|pod|PL)$/, keys %$manifest;
}
plan tests => scalar(@pod);
my $my_stopwords = <<'WORDS';
API
Addi
Addi's
Arnar
BMP
Blit
CGI
CMYK
CPAN
EXIF
FreeType
GDI
GIF
HSV
Hrafnkelsson
ICO
IMAGER
Imager
Imager's
JPEG
PNG
PNM
POSIX
RGB
RGBA
SGI
TGA
TIFF
UTF-8
Uncategorized
affine
allocator
bilevel
chromaticities
const
convolve
dpi
eg
equalities
gaussian
grayscale
ie
infix
invocant
metadata
multi-threaded
mutex
paletted
parsers
postfix
preload
preloading
preloads
quantization
quantize
quantized
radians
renderer
resizing
sRGB
specular
stereoscopy
tuple
unary
unassociated
unseekable
untransformed
varargs
WORDS

# see for example:
#  https://bugs.launchpad.net/ubuntu/+source/aspell/+bug/71322
$ENV{LANG} = "C";
$ENV{LC_ALL} = "C";
for my $file (@pod) {
  my $check_fh = File::Temp->new;
  my $check_filename = $check_fh->filename;
  my $work_fh = File::Temp->new;
  my $work_filename = $work_fh->filename;
  open my $pod, "<", $file
    or die "Cannot open $file for spell check: $!\n";
  my @local_stop;
  my $stop_re = qr/\b(xxxxx)\b/;
  while (<$pod>) {
    if (/^=for\s+stopwords\s+(.*)/) {
      push @local_stop, map quotemeta, split ' ', $1;
      my $stop_re_str = join('|', @local_stop);
      $stop_re = qr/\b($stop_re_str)\b/;
    }
    else {
      s/$stop_re/ /g;
    }
    print $work_fh $_;
  }
  seek $work_fh, 0, SEEK_SET;
  my $spell = Pod::Spell->new;
  $spell->stopwords->learn_stopwords($my_stopwords);
  $spell->parse_from_filehandle($work_fh, $check_fh);
  close $work_fh;
  close $check_fh;
  
  my @out = `aspell list <$check_filename`;
  unless (ok(@out == 0, "spell check $file")) {
    chomp @out;
    diag $_ for @out;
    print "#----\n";
    open my $fh, "<", $check_filename;
    while (<$fh>) {
      chomp;
      print "# $_\n";
    }
    print "#----\n";
  }
}
