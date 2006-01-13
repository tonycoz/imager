#!perl -w
# packaging test - make sure we included the samples in the MANIFEST <sigh>
use lib 't';
use Test::More;
use ExtUtils::Manifest qw(maniread);

# first build a list of samples from samples/README
open SAMPLES, "< samples/README"
  or die "Cannot open samples/README: $!";
my @sample_files;
while (<SAMPLES>) {
  chomp;
  /^\w[\w.-]+\.\w+$/ and push @sample_files, $_;
}

close SAMPLES;

plan tests => scalar(@sample_files);

my $manifest = maniread();

for my $filename (@sample_files) {
  ok(exists($manifest->{"samples/$filename"}), 
     "sample file $filename in manifest");
}
