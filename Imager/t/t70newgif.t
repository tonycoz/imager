# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)


BEGIN { $| = 1; print "1..3\n"; }
END {print "not ok 1\n" unless $loaded;}

use Imager;
$loaded=1;

print "ok 1\n";

Imager::init('log'=>'testout/t70newgif.log');

$green=i_color_new(0,255,0,0);
$blue=i_color_new(0,0,255,0);

$img=Imager->new();
$img->open(file=>'testimg/scale.ppm',type=>'pnm') || print "failed: ",$img->{ERRSTR},"\n";
print "ok 2\n";


$img->write(file=>'testout/t70newgif.gif',type=>'gif',gifplanes=>1,gifquant=>'lm',lmfixed=>[$green,$blue]) || print "failed: ",$img->{ERRSTR},"\n";
print "ok 3\n";











