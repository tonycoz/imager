# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..2\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;
#use Data::Dumper;
$loaded = 1;

print "ok 1\n";

init_log("testout/t00basicoo.log",1);

#list_formats();

%hsh=%Imager::formats;

print "# avaliable formats:\n";
for(keys %hsh) { print "# $_\n"; }

#print Dumper(\%hsh);

$img = Imager->new();

@all=qw(tiff gif jpg png ppm raw);

for(@all) {
  if (!$hsh{$_}) { next; }
  print "#opening Format: $_\n";
  if ($_ eq "raw") {
    $img->read(file=>"testout/t10.$_",type=>'raw', xsize=>150,ysize=>150) or die "failed: ",$img->{ERRSTR},"\n";
  } else {
    $img->read(file=>"testout/t10.$_") or die "failed: ",$img->{ERRSTR},"\n";
  }
}

$img2=$img->crop(width=>50,height=>50);
$img2->write(file=>'testout/t50.ppm',type=>'pnm');

undef($img);

malloc_state();


print "ok 2\n";
