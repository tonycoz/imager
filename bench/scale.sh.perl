#!perl -w
use strict;

print "$0\n";
my @allfiles = (@ARGV) x 120;
my $srcdir = '.';

for my $file (@allfiles) {
  # print STDERR "djpeg $srcdir/$file | pnmscale 0.33334 | cjpeg >med_res_pnm/$file\n";
  system("djpeg $srcdir/$file | pnmscale 0.33334 | cjpeg > med_res_pnm/$file");
  # print STDERR "djpeg med_res_pnm/$file | pnmscale 0.25 | cjpeg -quality 60 >icon_pnm/$file\n";
  system("djpeg med_res_pnm/$file | pnmscale 0.25 | cjpeg -quality 60 >icon_pnm/$file");
}
