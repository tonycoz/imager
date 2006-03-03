#!perl -w
use strict;
use Imager;

print "$0\n";
my @allfiles = (@ARGV) x 120;
my $srcdir = '.';

my %opts1 = (scalefactor=>.333334);
my %opts2 = (scalefactor=>.25);
my %exopts=();
for my $file (@allfiles) {
  # print STDERR "reading $srcdir/$file\n";
  my $img=Imager->new();
  $img->read(file=>"$srcdir/$file") or die "error on \"$srcdir/$file\":
".$img->{ERRSTR}."\n";
  # print STDERR "making med_res_imager/$file\n";
  my $scale=$img->scale(%opts1) or die "error on scale: ".$img->{ERRSTR};
  $scale->write(file=>"med_res_imager/$file",%exopts);
  # print STDERR "making icon_imager/$file\n";
  my $scale2=$scale->scale(%opts2);
  $scale2->write(file=>"icon_imager/$file",%exopts);
}
