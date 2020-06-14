#!perl -w
use strict;
use ExtUtils::Manifest qw(maniread);
use Test::More;

my $manifest = maniread();
my @pm = sort grep /\.pm$/ && !m(Devel/CheckLib.pm$), keys %$manifest;

for my $file (@pm) {
  open my $pm, "<", $file
    or die "Cannot open $file; $!";
  my $lines = join "", <$pm>;
  ok($lines =~ /^ *our \$VERSION/m, "$file: has a \$VERSION");
  ok($lines =~ /^use 5\.006;$/m, "$file: has use 5.006");
  ok($lines !~ /use vars/, "$file: hasn't any use vars");
}
done_testing();
