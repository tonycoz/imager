#!perl -w
use strict;
use Test::More;
use ExtUtils::Manifest qw(maniread);

# RT #74875
my $manifest = maniread();
my @files = grep /\.(c|im|h)$/, sort keys %$manifest;
plan tests => scalar @files;

++$|;
for my $file (@files) {
  my $ok = 1;
  if (open my $fh, "<", $file) {
    my $in_pod;
    while (<$fh>) {
      if (/^=cut/) {
	if ($in_pod) {
	  $in_pod = 0;
	}
	else {
	  diag("$file:$.: found =cut without pod");
	  $ok = 0;
	}
      }
      elsif (/^=\w+/) {
	$in_pod = $.;
      }
      elsif (m(\*/)) {
	if ($in_pod) {
	  diag("$file:$.: unclosed pod starting $in_pod");
	  $ok = 0;
	}
      }
    }
    close $fh;
  }
  else {
    diag("Cannot open $file: $!");
    $ok = 0;
  }
  ok($ok, $file);
}
