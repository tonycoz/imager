#!perl -w
use strict;
use Test::More tests => 43;
use Fcntl ':seek';

BEGIN { use_ok(Imager => ':all') };

init_log("testout/t07iolayer.log", 1);

undef($/);
# start by testing io buffer

my $data="P2\n2 2\n255\n 255 0\n0 255\n";
my $IO = Imager::io_new_buffer($data);
my $im = Imager::i_readpnm_wiol($IO, -1);

ok($im, "read from data io");

open(FH, ">testout/t07.ppm") or die $!;
binmode(FH);
my $fd = fileno(FH);
my $IO2 = Imager::io_new_fd( $fd );
Imager::i_writeppm_wiol($im, $IO2);
close(FH);
undef($im);

open(FH, "<testimg/penguin-base.ppm");
binmode(FH);
$data = <FH>;
close(FH);
my $IO3 = Imager::io_new_buffer($data);
#undef($data);
$im = Imager::i_readpnm_wiol($IO3, -1);

ok($im, "read from buffer, for compare");
undef $IO3;

open(FH, "<testimg/penguin-base.ppm") or die $!;
binmode(FH);
$fd = fileno(FH);
my $IO4 = Imager::io_new_fd( $fd );
my $im2 = Imager::i_readpnm_wiol($IO4, -1);
close(FH);
undef($IO4);

ok($im2, "read from file, for compare");

is(i_img_diff($im, $im2), 0, "compare images");
undef($im2);

my $IO5 = Imager::io_new_bufchain();
Imager::i_writeppm_wiol($im, $IO5);
my $data2 = Imager::io_slurp($IO5);
undef($IO5);

ok($data2, "check we got data from bufchain");

my $IO6 = Imager::io_new_buffer($data2);
my $im3 = Imager::i_readpnm_wiol($IO6, -1);

is(Imager::i_img_diff($im, $im3), 0, "read from buffer");

my $work = $data;
my $pos = 0;
sub io_reader {
  my ($size, $maxread) = @_;
  my $out = substr($work, $pos, $maxread);
  $pos += length $out;
  $out;
}
sub io_reader2 {
  my ($size, $maxread) = @_;
  my $out = substr($work, $pos, $maxread);
  $pos += length $out;
  $out;
}
my $IO7 = Imager::io_new_cb(undef, \&io_reader, undef, undef);
ok($IO7, "making readcb object");
my $im4 = Imager::i_readpnm_wiol($IO7, -1);
ok($im4, "read from cb");
ok(Imager::i_img_diff($im, $im4) == 0, "read from cb image match");

$pos = 0;
$IO7 = Imager::io_new_cb(undef, \&io_reader2, undef, undef);
ok($IO7, "making short readcb object");
my $im5 = Imager::i_readpnm_wiol($IO7, -1);
ok($im4, "read from cb2");
is(Imager::i_img_diff($im, $im5), 0, "read from cb2 image match");

sub io_writer {
  my ($what) = @_;
  substr($work, $pos, $pos+length $what) = $what;
  $pos += length $what;

  1;
}

my $did_close;
sub io_close {
  ++$did_close;
}

my $IO8 = Imager::io_new_cb(\&io_writer, undef, undef, \&io_close);
ok($IO8, "making writecb object");
$pos = 0;
$work = '';
ok(Imager::i_writeppm_wiol($im, $IO8), "write to cb");
# I originally compared this to $data, but that doesn't include the
# Imager header
ok($work eq $data2, "write image match");
ok($did_close, "did close");

# with a short buffer, no closer
my $IO9 = Imager::io_new_cb(\&io_writer, undef, undef, undef, 1);
ok($IO9, "making short writecb object");
$pos = 0;
$work = '';
ok(Imager::i_writeppm_wiol($im, $IO9), "write to short cb");
ok($work eq $data2, "short write image match");

{
  my $buf_data = "Test data";
  my $io9 = Imager::io_new_buffer($buf_data);
  is(ref $io9, "Imager::IO", "check class");
  my $work;
  is($io9->read($work, 4), 4, "read 4 from buffer object");
  is($work, "Test", "check data read");
  is($io9->read($work, 5), 5, "read the rest");
  is($work, " data", "check data read");
  is($io9->seek(5, SEEK_SET), 5, "seek");
  is($io9->read($work, 5), 4, "short read");
  is($work, "data", "check data read");
  is($io9->seek(-1, SEEK_CUR), 8, "seek relative");
  is($io9->seek(-5, SEEK_END), 4, "seek relative to end");
  is($io9->seek(-10, SEEK_CUR), -1, "seek failure");
  undef $io9;
}
{
  my $io = Imager::io_new_bufchain();
  is(ref $io, "Imager::IO", "check class");
  is($io->write("testdata"), 8, "check write");
  is($io->seek(-8, SEEK_CUR), 0, "seek relative");
  my $work;
  is($io->read($work, 8), 8, "check read");
  is($work, "testdata", "check data read");
  is($io->seek(-3, SEEK_END), 5, "seek end relative");
  is($io->read($work, 5), 3, "short read");
  is($work, "ata", "check read data");
  is($io->seek(4, SEEK_SET), 4, "absolute seek to write some");
  is($io->write("testdata"), 8, "write");
  is($io->seek(0, SEEK_CUR), 12, "check size");
  $io->close();
  
  # grab the data
  my $data = Imager::io_slurp($io);
  is($data, "testtestdata", "check we have the right data");
}
