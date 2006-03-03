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

Imager::init('log'=>'testout/t90cc.log');

$img=Imager->new();
$img->open(file=>'testimg/scale.ppm') || print "failed: ",$img->{ERRSTR},"\n";
print "ok 2\n";

print "# Less than 10K colors in image\n" if !defined($img->getcolorcount(maxcolors=>10000));
print "# color count: ".$img->getcolorcount()."\n";

print "ok 3\n";
