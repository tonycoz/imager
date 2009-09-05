#!perl -w
use strict;
$|=1;
use Test::More;
use Imager qw(:all);

i_has_format("gif")
  and plan skip_all => "gif support available and this tests the lack of it";

plan tests => 6;

my $im = Imager->new;
ok(!$im->read(file=>"testimg/scale.gif"), "should fail to read gif");
cmp_ok($im->errstr, '=~', "format 'gif' not supported", "check no gif message");
$im = Imager->new(xsize=>2, ysize=>2);
ok(!$im->write(file=>"testout/nogif.gif"), "should fail to write gif");
cmp_ok($im->errstr, '=~', "format 'gif' not supported", "check no gif message");
ok(!grep($_ eq 'gif', Imager->read_types), "check gif not in read types");
ok(!grep($_ eq 'gif', Imager->write_types), "check gif not in write types");
