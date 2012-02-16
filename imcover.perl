#!perl -w
use strict;
use Config;
use ExtUtils::Manifest 'maniread';
use Cwd;
use Getopt::Long;

my @tests;
my $verbose;
GetOptions("t|test=s" => \@tests,
	   "v" => \$verbose)
  or die;

my $make = $Config{make};
# if there's a way to make with profiling for a recursive build like
# Imager I don't see how
if (-f 'Makefile') {
  run("$make clean");
}
run("cover -delete");
run("perl Makefile.PL --coverage")
  and die;
run("$make 'OTHERLDFLAGS=-ftest-coverage -fprofile-arcs'")
  and die;

{
  local $ENV{DEVEL_COVER_OPTIONS} = "-db," . getcwd() . "/cover_db,-coverage,statement,branch,condition,subroutine";
  my $makecmd = "$make test TEST_VERBOSE=1 HARNESS_PERL_SWITCHES=-MDevel::Cover";
  if (@tests) {
    $makecmd .= " TEST_FILES='@tests'";
  }
  run($makecmd);
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
    run("cd $path ; gcov -abc @{$paths{$path}} ; cd ..");
  }
  else {
    run("gcov -abc @{$paths{$path}}");
  }
  my $dir = $path ? $path : '.';
  for my $file (@{$paths{$path}}) {
    run("gcov2perl $dir/$file.gcov");
  }
}

my @dbs = "cover_db", map "$_/cover_db", grep $_, keys %paths;
# we already ran gcov
run("cover -nogcov -ignore_re '^t/'");

sub run {
  my $cmd = shift;

  print "Running: $cmd\n" if $verbose;
  return system $cmd;
}
