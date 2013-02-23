#!perl -w
use strict;
use ExtUtils::Manifest qw(maniread);
use Test::More;
use File::Spec;

my @sub_dirs = qw(T1 FT2 W32 TIFF PNG GIF JPEG);

plan tests => scalar @sub_dirs;

my $base_mani = maniread();
my @base_mani = keys %$base_mani;
for my $sub_dir (@sub_dirs) {
  my @expected = map { my $x = $_; $x =~ s(^$sub_dir/)(); $x }
    grep /^$sub_dir\b/, @base_mani;
  push @expected,
    "MANIFEST", "MANIFEST.SKIP", "Changes", "inc/Devel/CheckLib.pm";
  @expected = sort @expected;

  my $found = maniread(File::Spec->catfile($sub_dir, "MANIFEST"));
  my @found = sort keys %$found;
  is_deeply(\@found, \@expected, "check sub-MANIFEST for $sub_dir");
}
