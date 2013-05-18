#!perl -w
use strict;
use Test::More;
use ExtUtils::Manifest qw(maniread);
use File::Temp;
eval "use Pod::Spell 1.01";
plan skip_all => "Pod::Spell 1.01 required for spellchecking POD" if $@;
my $manifest = maniread();
my @pod = sort grep !/^inc/ && /\.(pm|pl|pod|PL)$/, keys %$manifest;
plan tests => scalar(@pod);
my @stopwords = qw/
API
Addi
Addi's
Arnar
BMP
Blit
CGI
CMYK
CPAN
FreeType
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
/;

local %Pod::Wordlist::Wordlist = %Pod::Wordlist::Wordlist;
for my $stop (@stopwords) {
  $Pod::Wordlist::Wordlist{$stop} = 1;
}

# see for example:
#  https://bugs.launchpad.net/ubuntu/+source/aspell/+bug/71322
$ENV{LANG} = "C";
$ENV{LC_ALL} = "C";
for my $file (@pod) {
  my $check_fh = File::Temp->new;
  my $check_filename = $check_fh->filename;
  open POD, "< $file"
    or die "Cannot open $file for spell check: $!\n";
  Pod::Spell->new->parse_from_filehandle(\*POD, $check_fh);
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
