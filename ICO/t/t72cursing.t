#!perl -w
use strict;
use Test::More tests => 1;
use Imager;
require '../t/testtools.pl';

# checks that we load the CUR write handler automatically
my $img = test_oo_img();
ok($img->write(file => 'testout/cursing.cur'),
   "write cur with autoload")
  or print "# ",$img->errstr,"\n";
