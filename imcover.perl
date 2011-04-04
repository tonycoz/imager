#!perl -w
use strict;
use Config;
use ExtUtils::Manifest 'maniread';
use Cwd;

my $make = $Config{make};
# if there's a way to make with profiling for a recursive build like
# Imager I don't see how
if (-f 'Makefile') {
    system "$make clean";
}
system "cover -delete";
system "perl Makefile.PL --coverage"
  and die;
system "$make 'OTHERLDFLAGS=-ftest-coverage -fprofile-arcs'"
  and die;

{
  local $ENV{DEVEL_COVER_OPTIONS} = "-db," . getcwd() . "/cover_db";
  system "$make test TEST_VERBOSE=1 HARNESS_PERL_SWITCHES=-MDevel::Cover";
}

# build gcov files
my $mani = maniread();
# split by directory
my %paths;
for my $filename (keys %$mani) {
  next unless $filename =~ /\.(xs|c|im)$/;
  if ($filename =~ m!^(\w+)/(\w+\.\w+)$!) {
    push @{$paths{$1}}, $2;
  }
  else {
    push @{$paths{''}}, $filename;
  }
}

for my $path (keys %paths) {
  if ($path) {
    system "cd $path ; gcov @{$paths{$path}} ; cd ..";
  }
  else {
    system "gcov @{$paths{$path}}";
  }
  my $dir = $path ? $path : '.';
  for my $file (@{$paths{$path}}) {
    system "gcov2perl $dir/$file.gcov";
  }
}

my @dbs = "cover_db", map "$_/cover_db", grep $_, keys %paths;
system "cover -ignore_re '^t/'";

