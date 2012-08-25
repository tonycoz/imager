#!perl -w
use strict;
use threads;
use Imager;

++$|;
Imager->preload;

# as a TIFF this file is large, build it from largeish.jpg if it
# doesn't exist
unless (-f "bench/largish.tif") {
  my $im = Imager->new(file => "bench/largish.jpg")
    or die "Cannot read bench/largish.jpg:", Imager->errstr;
  $im->write(file => "bench/largish.tif")
    or die "Cannot write bench.largish.tif:", $im->errstr;
}

my @tests =
  (
   [ "bench/largish.tif", "" ],
   [ "TIFF/testimg/grey16.tif", "" ],
   [ "TIFF/testimg/comp4bad.tif", "(Iolayer): Read error at scanline 120; got 0 bytes, expected 32" ],
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
