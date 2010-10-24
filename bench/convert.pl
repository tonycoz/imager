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

addalpha: 1.3 /s (0.797143 / iter)
gray: 4.3 /s (0.233636 / iter)
green: 4.9 /s (0.205600 / iter)
noalpha: 1.6 /s (0.608889 / iter)

convert_via_copy:

addalpha: 4.9 /s (0.205600 / iter)
gray: 4.2 /s (0.235909 / iter)
green: 8.6 /s (0.115682 / iter)
noalpha: 5.4 /s (0.185556 / iter)

