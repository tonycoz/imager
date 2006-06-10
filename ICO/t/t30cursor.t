#!perl -w
use strict;
use Test::More tests => 14;

BEGIN { use_ok('Imager::File::CUR'); }

-d 'testout' or mkdir 'testout';

my $im = Imager->new;

ok($im->read(file => 'testimg/pal43232.cur', type=>'cur'),
   "read 4 bit");
is($im->getwidth, 32, "check width");
is($im->getheight, 32, "check width");
is($im->type, 'paletted', "check type");
is($im->tags(name => 'cur_bits'), 4, "check cur_bits tag");
is($im->tags(name => 'i_format'), 'cur', "check i_format tag");
is($im->tags(name => 'cur_hotspotx'), 1, "check cur_hotspotx tag");
is($im->tags(name => 'cur_hotspoty'), 18, "check cur_hotspoty tag");
my $mask = ".*" . ("\n" . "." x 32) x 32;
is($im->tags(name => 'cur_mask'), $mask, "check cur_mask tag");

# these should get pushed back into range on saving
$im->settag(name => 'cur_hotspotx', value => 32);
$im->settag(name => 'cur_hotspoty', value => -1);
ok($im->write(file=>'testout/hotspot.cur', type=>'cur'),
   "save with oor hotspot")
  or print "# ",$im->errstr, "\n";
my $im2 = Imager->new;
ok($im2->read(file=>'testout/hotspot.cur', type=>'cur'),
   "re-read the hotspot set cursor")
  or print "# ", $im->errstr, "\n";
is($im2->tags(name => 'cur_hotspotx'), 31, "check cur_hotspotx tag");
is($im2->tags(name => 'cur_hotspoty'), 0, "check cur_hotspoty tag");
