#!perl -w
use strict;
use lib '../t';
use Test::More tests => 1;
use Imager;
require '../t/testtools.pl';

# checks that we load the ICO write handler automatically
my $img = test_oo_img();
ok(Imager->write_multi({ file => 'testout/icomult.ico' }, $img, $img),
   "write_multi ico with autoload")
  or print "# ",Imager->errstr,"\n";
