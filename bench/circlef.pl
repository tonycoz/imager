#!perl -w
use strict;
use Benchmark qw(:hireswallclock countit);
use Imager;
use Imager::Fill;

#Imager->open_log(log => "circlef.log", loglevel => 2);

print $INC{"Imager.pm"}, "\n";

my $im = Imager->new(xsize => 1000, ysize => 1000);
my $im_pal = Imager->new(xsize => 1000, ysize => 1000, type => "paletted");
my @colors = map Imager::Color->new($_), qw/red green blue white black/;
$im_pal->addcolors(colors => \@colors);
my $color = $colors[0];
my $other = Imager::Color->new("pink");
my $fill = Imager::Fill->new(solid => "#ffff0080", combine => "normal");

countthese
  (5,
   {
    tiny_c_aa => sub {
      $im->circle(color => $color, r => 2, x => 20, y => 75, aa => 1)
	for 1 .. 100;
    },
    small_c_aa => sub {
      $im->circle(color => $color, r => 10, x => 20, y => 25, aa => 1)
	for 1 .. 100;
    },
    medium_c_aa => sub {
      $im->circle(color => $color, r => 60, x => 70, y => 80, aa => 1)
	for 1 .. 100;
    },
    large_c_aa => sub {
      $im->circle(color => $color, r => 400, x => 550, y => 580, aa => 1)
	for 1 .. 100;
    },

    tiny_fill_aa => sub {
      $im->circle(fill => $fill, r => 2, x => 20, y => 75, aa => 1)
	for 1 .. 100;
    },
    small_fill_aa => sub {
      $im->circle(fill => $fill, r => 10, x => 20, y => 25, aa => 1)
	for 1 .. 100;
    },
    medium_fill_aa => sub {
      $im->circle(fill => $fill, r => 60, x => 70, y => 80, aa => 1)
	for 1 .. 100;
    },
    large_fill_aa => sub {
      $im->circle(fill => $fill, r => 400, x => 550, y => 580, aa => 1)
	for 1 .. 100;
    },
   }
  );

#$im_pal->type eq "paletted" or die "Not paletted anymore";

sub countthese {
  my ($limit, $what) = @_;

  for my $key (sort keys %$what) {
    my $bench = countit($limit, $what->{$key});
    printf "$key: %.1f /s (%f / iter)\n", $bench->iters / $bench->cpu_p,
      $bench->cpu_p / $bench->iters;
  }
}

__END__

Original:

large_c_aa: 0.1 /s (13.110000 / iter)
large_fill_aa: 0.5 /s (2.026667 / iter)
medium_c_aa: 0.7 /s (1.402500 / iter)
medium_fill_aa: 1.9 /s (0.513000 / iter)
small_c_aa: 4.3 /s (0.230909 / iter)
small_fill_aa: 9.9 /s (0.100962 / iter)
tiny_c_aa: 17.4 /s (0.057444 / iter)
tiny_fill_aa: 25.7 /s (0.038947 / iter)

use a better algorithm for i_circle_aa() circle calculation:

large_c_aa: 1.3 /s (0.797143 / iter)
large_fill_aa: 0.5 /s (2.013333 / iter)
medium_c_aa: 32.2 /s (0.031012 / iter)
medium_fill_aa: 1.9 /s (0.523000 / iter)
small_c_aa: 166.8 /s (0.005993 / iter)
small_fill_aa: 9.9 /s (0.101373 / iter)
tiny_c_aa: 252.1 /s (0.003967 / iter)
tiny_fill_aa: 25.7 /s (0.038897 / iter)
