#!perl -w

# This file is for testing file functionality that is independent of
# the file format

use strict;
use lib 't';
use Test::More tests => 12;
use Imager;

Imager::init_log("testout/t1000files.log", 1);

SKIP:
{
  # Initally I tried to write this test using open to redirect files,
  # but there was a buffering problem that made it so the data wasn't
  # being written to the output file.  This external perl call avoids
  # that problem

  my $test_script = 'testout/t1000files_probe.pl';

  # build a temp test script to use
  ok(open(SCRIPT, "> $test_script"), "open test script")
    or skip("no test script $test_script: $!", 2);
  print SCRIPT <<'PERL';
#!perl
use Imager;
use strict;
my $file = shift or die "No file supplied";
open FH, "< $file" or die "Cannot open file: $!";
binmode FH;
my $io = Imager::io_new_fd(fileno(FH));
Imager::i_test_format_probe($io, -1);
PERL
  close SCRIPT;
  my $perl = $^X;
  $perl = qq/"$perl"/ if $perl =~ / /;
  
  print "# script: $test_script\n";
  my $cmd = "$perl -Mblib $test_script t/t1000files.t";
  print "# command: $cmd\n";

  my $out = `$cmd`;
  is($?, 0, "command successful");
  is($out, '', "output should be empty");
}

# test the file limit functions
# by default the limits are zero (unlimited)
print "# image file limits\n";
is_deeply([ Imager->get_file_limits() ], [0, 0, 0],
	  "check defaults");
ok(Imager->set_file_limits(width=>100), "set only width");
is_deeply([ Imager->get_file_limits() ], [100, 0, 0 ],
	  "check width set");
ok(Imager->set_file_limits(height=>150, bytes=>10000),
   "set height and bytes");
is_deeply([ Imager->get_file_limits() ], [ 100, 150, 10000 ],
	  "check all values now set");
ok(Imager->set_file_limits(reset=>1, height => 99),
   "set height and reset");
is_deeply([ Imager->get_file_limits() ], [ 0, 99, 0 ],
	  "check only height is set");
ok(Imager->set_file_limits(reset=>1),
   "just reset");
is_deeply([ Imager->get_file_limits() ], [ 0, 0, 0 ],
	  "check all are reset");
