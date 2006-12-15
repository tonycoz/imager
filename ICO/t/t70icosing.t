#!perl -w
use strict;
use Test::More tests => 1;
use Imager;
require '../t/testtools.pl';

# checks that we load the ICO write handler automatically
my $img = test_oo_img();
ok($img->write(file => 'testout/icosing.ico'),
   "write ico with autoload")
  or print "# ",$img->errstr,"\n";
