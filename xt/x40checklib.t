#!perl -w
#
# Each sub-module ships with our custom Devel::CheckLib, make sure
# they all match
use strict;
use Test::More;

my @subs = qw(FT2 GIF JPEG PNG T1 TIFF W32);

plan tests => 1 + @subs;

# load the base file

my $base = load("inc/Devel/CheckLib.pm");

ok($base, "Loaded base file");

for my $sub (@subs) {
  my $data = load("$sub/inc/Devel/CheckLib.pm");

  # I'd normally use is() here, but it's excessively noisy when
  # comparing this size of data
  ok(defined($data) && $data eq $base, "check $sub");
}

sub load {
  my ($filename) = @_;

  if (open my $f, "<", $filename) {
    my $data = do { local $/; <$f> };
    close $f;

    return $data;
  }
  else {
    diag "Cannot load $filename: $!\n";
    return;
  }
}
