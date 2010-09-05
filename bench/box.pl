#!perl -w
use strict;
use Benchmark qw(:hireswallclock countit);
use Imager;

my $im = Imager->new(xsize => 1000, ysize => 1000);
my $im_pal = Imager->new(xsize => 1000, ysize => 1000, type => "paletted");
my @colors = map Imager::Color->new($_), qw/red green blue white black/;
$im_pal->addcolors(colors => \@colors);
my $color = $colors[0];
my $other = Imager::Color->new("pink");

countthese
  (5,
   {
    box0010 => sub {
      $im->box(xmax => 10, ymax => 10, color => $other)
	for 1 .. 100;
    },
    box0010c => sub {
      $im->box(xmax => 10, ymax => 10, color => "pink")
	for 1 .. 100;
    },
    box0010d => sub {
      $im->box(xmax => 10, ymax => 10)
	for 1 .. 100;
    },
    box0100 => sub {
      $im->box(xmax => 100, ymax => 100, color => $other)
	for 1 .. 100;
    },
    box0500 => sub {
      $im->box(xmax => 500, ymax => 500, color => $other)
	for 1 .. 100;
    },
    box1000 => sub {
      $im->box(color => $other)
	for 1 .. 100;
    },
    palbox0010 => sub {
      $im_pal->box(xmax => 10, ymax => 10, color => $color)
	for 1 .. 100;
    },
    palbox0100 => sub {
      $im_pal->box(xmax => 100, ymax => 100, color => $color)
	for 1 .. 100;
    },
    palbox0500 => sub {
      $im_pal->box(xmax => 500, ymax => 500, color => $color)
	for 1 .. 100;
    },
    palbox1000 => sub {
      $im_pal->box(color => $color)
	for 1 .. 100;
    },

    fbox0010 => sub {
      $im->box(xmax => 10, ymax => 10, filled => 1, color => $other)
	for 1 .. 100;
    },
    fbox0010c => sub {
      $im->box(xmax => 10, ymax => 10, filled => 1, color => "pink")
	for 1 .. 100;
    },
    fbox0010d => sub {
      $im->box(xmax => 10, ymax => 10, filled => 1)
	for 1 .. 100;
    },
    fbox0100 => sub {
      $im->box(xmax => 100, ymax => 100, filled => 1, color => $other)
	for 1 .. 100;
    },
    fbox0500 => sub {
      $im->box(xmax => 500, ymax => 500, filled => 1, color => $other)
	for 1 .. 100;
    },
    fbox1000 => sub {
      $im->box(color => $other, filled => 1)
	for 1 .. 100;
    },
    fpalbox0010 => sub {
      $im_pal->box(xmax => 10, ymax => 10, filled => 1, color => $color)
	for 1 .. 100;
    },
    fpalbox0100 => sub {
      $im_pal->box(xmax => 100, ymax => 100, filled => 1, color => $color)
	for 1 .. 100;
    },
    fpalbox0500 => sub {
      $im_pal->box(xmax => 500, ymax => 500, filled => 1, color => $color)
	for 1 .. 100;
    },
    fpalbox1000 => sub {
      $im_pal->box(filled => 1, color => $color)
	for 1 .. 100;
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

box0010: 397.7 /s (0.002514 / iter)
box0010c: 3.3 /s (0.305882 / iter)
box0010d: 399.6 /s (0.002502 / iter)
box0100: 329.5 /s (0.003035 / iter)
box0500: 191.5 /s (0.005223 / iter)
box1000: 130.6 /s (0.007657 / iter)
fbox0010: 372.3 /s (0.002686 / iter)
fbox0010c: 3.3 /s (0.300588 / iter)
fbox0010d: 383.2 /s (0.002610 / iter)
fbox0100: 63.8 /s (0.015685 / iter)
fbox0500: 2.8 /s (0.361429 / iter)
fbox1000: 0.7 /s (1.435000 / iter)
fpalbox0010: 370.9 /s (0.002696 / iter)
fpalbox0100: 53.2 /s (0.018799 / iter)
fpalbox0500: 2.4 /s (0.413077 / iter)
fpalbox1000: 0.6 /s (1.700000 / iter)
palbox0010: 390.2 /s (0.002563 / iter)
palbox0100: 316.9 /s (0.003155 / iter)
palbox0500: 171.8 /s (0.005820 / iter)
palbox1000: 115.0 /s (0.008694 / iter)

Re-work sub box():

box0010: 786.0 /s (0.001272 / iter)
box0010c: 3.3 /s (0.300000 / iter)
box0010d: 463.7 /s (0.002157 / iter)
box0100: 556.6 /s (0.001797 / iter)
box0500: 254.4 /s (0.003930 / iter)
box1000: 154.8 /s (0.006460 / iter)
fbox0010: 700.9 /s (0.001427 / iter)
fbox0010c: 3.3 /s (0.302353 / iter)
fbox0010d: 437.0 /s (0.002288 / iter)
fbox0100: 69.2 /s (0.014444 / iter)
fbox0500: 2.8 /s (0.357143 / iter)
fbox1000: 0.7 /s (1.437500 / iter)
fpalbox0010: 673.5 /s (0.001485 / iter)
fpalbox0100: 46.8 /s (0.021377 / iter)
fpalbox0500: 2.0 /s (0.505000 / iter)
fpalbox1000: 0.5 /s (2.140000 / iter)
palbox0010: 740.9 /s (0.001350 / iter)
palbox0100: 473.2 /s (0.002113 / iter)
palbox0500: 186.2 /s (0.005371 / iter)
palbox1000: 109.1 /s (0.009167 / iter)
