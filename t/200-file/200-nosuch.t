#!perl -w
use strict;
use Imager;
use Test::More;

# test handling of unknown file formats

my $log_file = "testout/file-200-nosuch.log";

Imager->open_log(log => $log_file);

my $im = Imager->new;
ok(!$im->read(file => "testimg/base.jpg", type=> "unknown"),
   "check we can't read 'unknown' format files");
like($im->errstr, qr/format 'unknown' not supported/,
     "check no unknown message");
$im = Imager->new(xsize => 2, ysize => 2);
ok(!$im->write(file => "testout/nosuch.unknown", type => "unknown"),
   "check we can't write 'unknown' format files");
like($im->errstr, qr/format 'unknown' not supported/,
     "check no 'unknown' message");

ok(!grep($_ eq "unknown", Imager->read_types),
   "check unknown not in read types");
ok(!grep($_ eq "unknown", Imager->write_types),
   "check unknown not in write types");

done_testing();

Imager->close_log;

unless ($ENV{IMAGER_KEEP_FILES}) {
  unlink $log_file;
}
