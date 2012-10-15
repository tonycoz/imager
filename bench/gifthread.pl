#!perl -w
use strict;
use threads;
use Imager;

++$|;
Imager->preload;

# as a TIFF this file is large, build it from largeish.jpg if it
# doesn't exist
unless (-f "bench/largish.gif") {
  my $im = Imager->new(file => "bench/largish.jpg")
    or die "Cannot read bench/largish.jpg:", Imager->errstr;
  $im->write(file => "bench/largish.gif")
    or die "Cannot write bench/largish.gif:", $im->errstr;
}

my @tests =
  (
   [ "bench/largish.gif", "" ],
   [ "GIF/testimg/scale.gif", "" ],
   [ "GIF/testimg/nocmap.gif", "Image does not have a local or a global color map" ],
  );

my @threads;
my $name = "A";
for my $test (@tests) {
  push @threads,
    threads->create
      (
       sub  {
	 my ($file, $result, $name) = @_;
	 for (1 .. 100000) {
	   print $name;
	   my $im = Imager->new(file => $file);
	   if ($result) {
	     $im and die "Expected error from $file, got image";
	     Imager->errstr eq $result
	       or die "Expected error '$result', got '",Imager->errstr, "'"
	   }
	   else {
	     $im or die "Expected image got error '", Imager->errstr, "'";
	   }
	 }
	 return;
       },
       @$test,
       $name
      );
  ++$name;
}

for my $t (@threads) {
  $t->join();
}
