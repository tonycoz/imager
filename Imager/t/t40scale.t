# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..6\n"; }
END {print "not ok 1\n" unless $loaded;}

use Imager qw(:all);

$loaded = 1;

Imager::init('log'=>'testout/t40scale.log');
print "ok 1\n";

$img=Imager->new();

$img->open(file=>'testimg/scale.ppm',type=>'pnm') or print "failed: ",$scaleimg->{ERRSTR},"\n";	
print "ok 2\n";	

$scaleimg=$img->scale(scalefactor=>0.25) or print "failed: ",$scaleimg->{ERRSTR},"\n";
print "ok 3\n";

$scaleimg->write(file=>'testout/t40scale1.ppm',type=>'pnm') or print "failed: ",$scaleimg->{ERRSTR},"\n";
print "ok 4\n";

$scaleimg=$img->scale(scalefactor=>0.25,qtype=>'preview') or print "failed: ",$scaleimg->{ERRSTR},"\n";
print "ok 5\n";

$scaleimg->write(file=>'testout/t40scale2.ppm',type=>'pnm') or print "failed: ",$scaleimg->{ERRSTR},"\n";
print "ok 6\n";


