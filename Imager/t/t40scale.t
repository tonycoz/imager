#!perl -w
use strict;
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)
my $loaded;

BEGIN { $| = 1; print "1..12\n"; }
END {print "not ok 1\n" unless $loaded;}

use Imager qw(:all);

$loaded = 1;

require "t/testtools.pl";

Imager::init('log'=>'testout/t40scale.log');
print "ok 1\n";

my $img=Imager->new();

$img->open(file=>'testimg/scale.ppm',type=>'pnm') or print "failed: ",$img->{ERRSTR},"\n";	
print "ok 2\n";	

my $scaleimg=$img->scale(scalefactor=>0.25) or print "failed: ",$img->{ERRSTR},"\n";
print "ok 3\n";

$scaleimg->write(file=>'testout/t40scale1.ppm',type=>'pnm') or print "failed: ",$scaleimg->{ERRSTR},"\n";
print "ok 4\n";

$scaleimg=$img->scale(scalefactor=>0.25,qtype=>'preview') or print "failed: ",$scaleimg->{ERRSTR},"\n";
print "ok 5\n";

$scaleimg->write(file=>'testout/t40scale2.ppm',type=>'pnm') or print "failed: ",$scaleimg->{ERRSTR},"\n";
print "ok 6\n";

{
  # check for a warning when scale() is called in void context
  my $warning;
  local $SIG{__WARN__} = 
    sub { 
      $warning = "@_";
      my $printed = $warning;
      $printed =~ s/\n$//;
      $printed =~ s/\n/\n\#/g; 
      print "# ",$printed, "\n";
    };
  $img->scale(scalefactor=>0.25);
  print $warning =~ /void/ ? "ok 7\n" : "not ok 7\n";
  print $warning =~ /t40scale\.t/ ? "ok 8\n" : "not ok 8\n";
}
{ # https://rt.cpan.org/Ticket/Display.html?id=7467
  # segfault in Imager 0.43
  # make sure scale() doesn't let us make an image zero pixels high or wide
  # it does this by making the given axis as least 1 pixel high
  my $out = $img->scale(scalefactor=>0.00001);
  print $out->getwidth == 1 ? "ok 9\n" : "not ok 9\n";
  print $out->getheight == 1 ? "ok 10\n" : "not ok 10\n";

  $out = $img->scale(scalefactor=>0.00001, qtype => 'preview');
  print $out->getwidth == 1 ? "ok 11\n" : "not ok 11\n";
  print $out->getheight == 1 ? "ok 12\n" : "not ok 12\n";
}
