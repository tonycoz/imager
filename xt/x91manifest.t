#!perl -w
use strict;
use ExtUtils::Manifest qw(maniread fullcheck manifind maniskip);
use Test::More;
use File::Spec;
use Cwd qw(getcwd);

my @sub_dirs = qw(FT2 GIF JPEG PNG T1 TIFF W32);

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

my $cwd = getcwd;

for my $dir (".", @sub_dirs) {
  ok(chdir $dir, "chdir $dir");
  my $have = maniread();
  my @have = sort keys %$have;
  my $skipchk = maniskip();
  my @expect = grep !$skipchk->($_), sort keys %{+manifind()};
  unless (is_deeply(\@have, \@expect, "$dir: check manifind vs MANIFEST")) {
    my %have = map { $_ => 1 } @have;
    my %expect = map { $_ => 1 } @expect;
    my @extra = grep !$have{$_}, @expect;
    my @missing = grep !$expect{$_}, @have;
    if (@extra) {
      diag "Extra:";
      diag "  $_" for @extra;
    }
    if (@missing) {
      diag "Missing:";
      diag "  $_" for @missing;
    }
  }
  # my ($missing, $extra) = fullcheck;
  # is_deeply($missing, [], "dir $dir: check no missing files");
  # is_deeply($extra, [], "dir $dir: check no extra files");
  ok(chdir $cwd, "return to $cwd");
}

done_testing();
