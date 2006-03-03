#!perl -w
use strict;
use Data::Dumper;

print <<EOS;
This program benchmarks various RGB to palette index code segments (quant.c).

The idea is to run it on various processors to see which is fastest on
average.

It takes about 50 minutes on my 700MHz PIII laptop.

If you want to see what the output images look like, cd to .. and run:

  perl -Mblib bench/quantone.perl foo

which gives the output files a prefix of 'foo'.

EOS

-e 'hsvgrad.png' && -e 'rgbtile.png'
  or die "Run makegrad.perl first";
-e 'kscdisplay.png' 
  or die "Missing file. Download http://www.develop-help.com/imager/kscdisplay.png";
chdir ".." or die "Can't chdir to parent: $!";

#my %qopts = qw(linsearch IM_CFLINSEARCH
#               hashbox   IM_CFHASHBOX
#               sortchan  IM_CFSORTCHAN
#               rand2dist IM_CFRAND2DIST);

my %qopts = qw(hashbox   IM_CFHASHBOX
               rand2dist IM_CFRAND2DIST);


# $bench{$opt}{$image}{$tran}{$pal} = time
my %bench;
$|++;
unlink "bench/quantbench.txt";
for my $opt (keys %qopts) {
  open LOG, ">> bench/quantbench.log" or die "Cannot open log: $!";
  print LOG "*** $opt ***\n";
  close LOG;
  $ENV{IM_CFLAGS} = "-DIM_CF_COPTS -D$qopts{$opt}";
  print "*** $opt configuring";
  system "perl Makefile.PL >>bench/quantbench.log 2>&1"
    and die "Failed to configure Imager";
  print " building";
  system "make >>bench/quantbench.log 2>&1"
    and die "Failed to build Imager";
  print " benchmarking";
  my @out = `perl -Mblib bench/quantone.perl`;
  $? and die "Failed to run benchmark: $?";
  chomp @out;
  print "parsing\n";
  my ($image, $tran);
  foreach (@out) {
    if (/^\*\*\s+(\S+)\s+(\S+)/) {
      $image = $1;
      $tran = $2;
    }
    elsif (/^\s*(\w+).*\@\s*([\d.]+)/ or /^\s*(\w+?):.+?([\d.]+?) CPU\)/) {
			print "$1: $2\n";
      $bench{$opt}{$image}{$tran}{$1} = $2;
    }
    elsif (/^Benchmark:/) {
      # ignored
    }
    else {
      die "Unknown benchmark output: $_\n";
    }
  }
}

system "uname -srmp >bench/quantbench.txt"; # is -srmp portable?
open BENCH, ">> bench/quantbench.txt" 
  or die "Cannot open bench/quantbench.txt: $!";
print BENCH Dumper(\%bench);
close BENCH or die "Cannot close benchmark file: $!";
print "Please email bench/quantbench.txt to tony\@develop-help.com\n";

__END__

=head1 NAME

quantbench.pl - Benchmarks Imager's image quantization code.

=head1 SYNOPSIS

  perl makegrad.pl
  perl quantbench.pl

=head1 DESCRIPTION

Builds Imager with various quantization code options, and then times
the results of running with those options, with various palettes and
options.

The aim is to run this with several options on several platforms, and
see which produces the fastest results overall.

Requires that PNG and GIF (or ungif) support is present.

=cut

