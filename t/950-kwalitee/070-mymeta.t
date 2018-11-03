#!perl -w
use strict;
use Test::More;
plan skip_all => "Only run as part of the dist"
  unless -f "MYMETA.json";
eval "use CPAN::Meta 2.110580;";
plan skip_all => "CPAN::Meta required for testing META.json"
  if $@;
plan skip_all => "Only if automated or author testing"
  unless $ENV{AUTOMATED_TESTING} || -d ".git";
plan tests => 1;

my $meta;
unless (ok(eval {
  $meta = CPAN::Meta->load_file("MYMETA.json",
				{ lazy_validation => 0 }) },
	   "loaded MYMETA.json successfully")) {
  diag($@);
}
