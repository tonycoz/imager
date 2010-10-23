#!perl -w
use strict;
use Benchmark qw(:hireswallclock countit);
use Imager;

my $im = Imager->new(xsize => 1000, ysize => 1000);
my $ima = $im->convert(preset => "addalpha");
my $im_pal = Imager->new(xsize => 1000, ysize => 1000, type => "paletted");
my @colors = map Imager::Color->new($_), qw/red green blue white black/;
$im_pal->addcolors(colors => \@colors);
my $color = $colors[0];
my $other = Imager::Color->new("pink");

countthese
  (5,
   {
    gray => sub {
      my $temp = $im->convert(preset => "grey")
	for 1 .. 10;
    },
    green => sub {
      my $temp = $im->convert(preset => "green")
	for 1 .. 10;
    },
    addalpha => sub {
      my $temp = $im->convert(preset => "addalpha")
	for 1 .. 10;
    },
    noalpha => sub {
      my $temp = $ima->convert(preset => "noalpha")
	for 1 .. 10;
    },
   }
  );

$im_pal->type eq "paletted" or die "Not paletted anymore";

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

addalpha: 0.9 /s (1.082000 / iter)
gray: 3.3 /s (0.303529 / iter)
green: 4.1 /s (0.244286 / iter)
noalpha: 1.1 /s (0.876667 / iter)

