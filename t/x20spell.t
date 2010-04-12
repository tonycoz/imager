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
Imager
Imager's
IMAGER
GIF
JPEG
PNG
TIFF
BMP
SGI
TGA
RGB
ICO
PNM
bilevel
dpi
Arnar
Hrafnkelsson
API
paletted
guassian
metadata
CPAN
eg
ie
CMYK
HSV
CGI
const
varargs
FreeType
UTF-8
RGBA
postfix
infix
unary
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
    print "# $_\n" for @out;
    print "#----\n";
    open my $fh, "<", $check_filename;
    while (<$fh>) {
      chomp;
      print "# $_\n";
    }
    print "#----\n";
  }
}
