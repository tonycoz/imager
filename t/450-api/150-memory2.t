#!perl -w
#
# this tests both the Inline interface and the API
use strict;
use Test::More;
use Imager;
use Config;

plan skip_all => "Skipping risky tests"
  unless $ENV{IMAGER_RISKY_TESTS};
plan skip_all => "We might not fail on 32-bit"
  unless $Config{ptrsize} < 8;

eval "require Inline::C;";
plan skip_all => "Inline required for testing API" if $@;

eval "require Parse::RecDescent;";
plan skip_all => "Could not load Parse::RecDescent" if $@;

use Cwd;
plan skip_all => "Inline won't work in directories with spaces"
  if getcwd() =~ / /;

-d "testout" or mkdir "testout";

Imager->open_log(log => "testout/140-memory1.log", loglevel => 4);

print STDERR "Inline version $Inline::VERSION\n";

require Inline;
Inline->import(with => 'Imager');
Inline->import("FORCE"); # force rebuild
#Inline->import(C => Config => OPTIMIZE => "-g");

Inline->bind(C => <<'EOS');
void
alloc_lots() {
  void *p = mymalloc(~(size_t)0 / 2 - 10000);
}
EOS

ok(!Imager->set_out_of_memory_handler(),
   "no handler by default");

Imager->set_out_of_memory_handler
  (
    sub {
      my ($func, $size) = @_;
      note "Out of memory $func $size";
      die "Out of memory $func $size";
      exit;
    });

ok(!eval {
  alloc_lots();
  1;
}, "croak on out of memory");
like($@, qr/Out of memory mymalloc \d+/, "message");
ok(Imager->set_out_of_memory_handler(), "now there is one");
done_testing();
Imager->close_log();
