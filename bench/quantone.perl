#!perl -w
use Imager;
use Benchmark;

# actual benchmarking code for quantbench.pl - not intended to be used
# directly

my %imgs;

my $out = shift;

# rgbtile and hsvgrad are both difficult images - they both have
# more than 256 colours
my $img = Imager->new;
$img->open(file=>'bench/rgbtile.png')
  or die "Cannot load bench/rgbtile.png:",$img->errstr;
$imgs{rgbtile} = $img;

$img = Imager->new;
$img->open(file=>'bench/hsvgrad.png')
  or die "Cannot load bench/hsvgrad.png:", $img->errstr;
$imgs{hsvgrad} = $img;

$img = Imager->new;
$img->open(file=>'bench/kscdisplay.png')
  or die "Cannot load bench/kscdisplay.png:", $img->errstr;
$imgs{kscdisplay} = $img;

# I need some other images
for my $key (keys %imgs) {
  for my $tran (qw(closest errdiff)) {
    my $img = $imgs{$key};
    print "** $key $tran\n";
    timethese(10,
	      {
	       addi=>sub {
		 $img->write(file=>out($out, $key, $tran, 'addi'), type=>'gif',
			     gifquant=>'gen', make_colors=>'addi',
			    translate=>$tran)
		   or die "addi",$img->errstr;
	       },
	       webmap=>sub {
		 $img->write(file=>out($out, $key, $tran, 'webmap'), 
			     type=>'gif',
			     gifquant=>'gen', make_colors=>'webmap',
			    translate=>$tran)
		   or die "webmap",$img->errstr;
	       },
	       mono=>sub {
		 $img->write(file=>out($out, $key, $tran, 'mono'), type=>'gif',
			     gifquant=>'gen', make_colors=>'none',
			     colors=>[Imager::Color->new(0,0,0),
				      Imager::Color->new(255,255,255) ],
			    translate=>$tran)
		   or die "mono",$img->errstr;
	       },
	      });
  }
}

sub out {
  my ($out, $in, $tran, $pal) = @_;
  $out or return '/dev/null';
  return "bench/${out}_${in}_${tran}_$pal.gif";
}

__END__

=head1 NAME

quantone.perl - benchmarks image quantization with various options

=head1 SYNOPSIS

  # just benchmark
  perl bench/quantone.perl
  # produce output images too
  perl bench/quantone.perl prefix

=head1 DESCRIPTION

Benchmarks image quantization on some test images, and with various
options.

The current images are 2 synthesized images (rgbtile.png and
hsvgrad.png), and a cropped photo (kscdisplay.png).

This program is designed to be run by L<quantbench.perl>.

=cut
