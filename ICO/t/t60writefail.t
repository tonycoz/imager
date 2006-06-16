#!perl -w
use strict;
use Test::More tests => 24;
use Imager;

# this file tries to cover as much of the write error handling cases in 
# msicon.c/imicon.c as possible.
#
# coverage checked with gcc/gcov

# image too big for format tests, for each entry point
{
  my $im = Imager->new(xsize => 256, ysize => 255);
  my $data;
  ok(!$im->write(data => \$data, type=>'ico'),
     "image too large");
  is($im->errstr, "image too large for ico file", "check message");
}

{
  my $im = Imager->new(xsize => 256, ysize => 255);
  my $data;
  ok(!Imager->write_multi({ data => \$data, type=>'ico' }, $im, $im),
     "image too large");
  is(Imager->errstr, "image too large for ico file", "check message");
  Imager->_set_error('');
}

{
  my $im = Imager->new(xsize => 256, ysize => 255);
  my $data;
  ok(!$im->write(data => \$data, type=>'cur'),
     "image too large");
  is($im->errstr, "image too large for ico file", "check message");
}

{
  my $im = Imager->new(xsize => 256, ysize => 255);
  my $data;
  ok(!Imager->write_multi({ data => \$data, type=>'cur' }, $im),
     "image too large");
  is(Imager->errstr, "image too large for ico file", "check message");
  Imager->_set_error('');
}

# low level write failure tests for each entry point (fail on close)
{
  my $im = Imager->new(xsize => 10, ysize => 10);
  ok(!$im->write(callback => \&write_failure, type=>'ico'),
     "low level write failure (ico)");
  is($im->errstr, "error closing output: synthetic error", "check message");
}

{
  my $im = Imager->new(xsize => 10, ysize => 10);
  ok(!$im->write(callback => \&write_failure, type=>'cur'),
     "low level write failure (cur)");
  is($im->errstr, "error closing output: synthetic error", "check message");
}

{
  my $im = Imager->new(xsize => 10, ysize => 10);
  ok(!Imager->write_multi({ callback => \&write_failure, type=>'ico' }, $im, $im),
     "low level write_multi failure (ico)");
  is(Imager->errstr, "error closing output: synthetic error", "check message");
  Imager->_set_error('');
}

{
  my $im = Imager->new(xsize => 10, ysize => 10);
  ok(!Imager->write_multi({ callback => \&write_failure, type=>'cur' }, $im, $im),
     "low level write_multi failure (cur)");
  is(Imager->errstr, "error closing output: synthetic error", "check message");
  Imager->_set_error('');
}

# low level write failure tests for each entry point (fail on write)
{
  my $im = Imager->new(xsize => 10, ysize => 10);
  ok(!$im->write(callback => \&write_failure, maxbuffer => 1, type=>'ico'),
     "low level write failure (ico)");
  is($im->errstr, "Write failure: synthetic error", "check message");
}

{
  my $im = Imager->new(xsize => 10, ysize => 10);
  ok(!$im->write(callback => \&write_failure, maxbuffer => 1, type=>'cur'),
     "low level write failure (cur)");
  is($im->errstr, "Write failure: synthetic error", "check message");
}

{
  my $im = Imager->new(xsize => 10, ysize => 10);
  ok(!Imager->write_multi({ callback => \&write_failure, maxbuffer => 1, type=>'ico' }, $im, $im),
     "low level write_multi failure (ico)");
  is(Imager->errstr, "Write failure: synthetic error", "check message");
  Imager->_set_error('');
}

{
  my $im = Imager->new(xsize => 10, ysize => 10);
  ok(!Imager->write_multi({ callback => \&write_failure, maxbuffer => 1, type=>'cur' }, $im, $im),
     "low level write_multi failure (cur)");
  is(Imager->errstr, "Write failure: synthetic error", "check message");
  Imager->_set_error('');
}

# write callback that fails
sub write_failure {
  print "# synthesized write failure\n";
  Imager::i_push_error(0, "synthetic error");
  return;
}
