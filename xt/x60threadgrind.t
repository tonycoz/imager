#!perl -w
use strict;
use Imager;
use Imager::Color::Float;
use Imager::Fill;
use Config;
my $loaded_threads;
BEGIN {
  if ($Config{useithreads} && $] > 5.008007) {
    $loaded_threads =
      eval {
	require threads;
	threads->import;
	1;
      };
  }
}
use Test::More;

$Config{useithreads}
  or plan skip_all => "can't test Imager's threads support with no threads";
$] > 5.008007
  or plan skip_all => "require a perl with CLONE_SKIP to test Imager's threads support";
$loaded_threads
  or plan skip_all => "couldn't load threads";

$INC{"Devel/Cover.pm"}
  and plan skip_all => "threads and Devel::Cover don't get along";

# https://rt.cpan.org/Ticket/Display.html?id=65812
# https://github.com/schwern/test-more/issues/labels/Test-Builder2#issue/100
$Test::More::VERSION =~ /^2\.00_/
  and plan skip_all => "threads are hosed in 2.00_06 and presumably all 2.00_*";

plan tests => 1;

Imager->preload;

++$|;

{
  my @t;
  for (1 .. 10) {
    push @t, threads->create
      (
       sub {
	 my ($which) = @_;
	 my @im;
	 print "# start $which\n";
	 for (1 .. 1000) {
	   push @im, Imager->new(xsize => 10, ysize => 10);
	   $im[0]->box(fill => { solid => "#FF0000" });
	   my $im = Imager->new(xsize => 10, ysize => 10, type => "paletted");
	   $im->setpixel(x => 5, y => 5, color => "#808080");
	 }
	 print "# end $which\n";
       },
       $_
      );
  }
  $_->join for @t;
  pass("grind image creation");
}
