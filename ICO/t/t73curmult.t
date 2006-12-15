#!perl -w
use strict;
use Test::More tests => 1;
use Imager;
require '../t/testtools.pl';

# checks that we load the CUR write handler automatically
my $img = test_oo_img();
ok(Imager->write_multi({ file => 'testout/icomult.cur' }, $img, $img),
   "write_multi cur with autoload")
  or print "# ",Imager->errstr,"\n";
