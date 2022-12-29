package ImageStuff;
use 5.006;
use strict;
use warnings;
use POSIX qw(floor);
use Imager;

our $VERSION = "1.000";

sub _scale_horiz {
  my ($in, $newlen) = @_;

  my @out;
  my $start = 0;
  my $fstart = 0;
  my $scale = $newlen / @$in;
  for my $x (0 .. $newlen-1) {
    my $end = @$in * ($x+1) / $newlen;

    my $fend = floor($end);

    if ($fstart == $fend) {
      $out[$x] = $in->[$fstart] * ($end - $start) * $scale;
    }
    else {
      my $work = 0;
      $work = ($fstart + 1 - $start) * $in->[$fstart];
      for my $i ($fstart + 1 .. $fend - 1) {
        $work += $in->[$i];
      }
      if ($fend < @$in) {
        $work += ($end - $fend) * $in->[$fend];
      }
      $out[$x] = $work * $scale;
    }

    $start = $end;
    $fstart = $fend;
  }

  return \@out;
}

sub scale_mixing {
  my ($class, $im, $width, $height) = @_;

  $im->alphachannel
    and die "No alpha channel support";

  my $out = $im->_sametype(xsize => $width, ysize => $height);

  for my $ch (0 .. $im->getchannels()-1) {
    my @hscaled;
    for my $y (0 .. $im->getheight()-1) {
      push @hscaled, _scale_horiz([ $im->getsamples(y => $y, scale => "linear", channels => [ $ch ], type => "16bit") ],
                                 $width);
    }
    my $start = 0;
    my $fstart = 0;
    my $oldheight = $im->getheight;
    my $scale = $height / $oldheight;
    for my $y (0 .. $height-1) {
      my @out;
      my $end = $oldheight * ($y+1) / $height;
      my $fend = floor($end);

      if ($fstart == $fend) {
        my $diff;
        @out = map { $_ * $diff * $scale } @{$hscaled[$fstart]};
      }
      else {
        my $dstart = ($fstart + 1 - $start);
        @out = map { $dstart * $_ } @{$hscaled[$fstart]};
        for my $row ($fstart + 1 .. $fend - 1) {
          my $hrow = $hscaled[$row];
          for my $i (0 .. $width-1) {
            $out[$i] += $hrow->[$i];
          }
        }
        my $dend = $end - $fend;
        if ($dend > 0.00001) {
          my $erow = $hscaled[$fend];
          for my $i (0 .. $width - 1) {
            $out[$i] += $erow->[$i] * $dend;
          }
        }
        for my $samp (@out) {
          $samp *= $scale;
        }
      }

      $start = $end;
      $fstart = $fend;

      $out->setsamples(y => $y, scale => "linear", channels => [ $ch ], type => "16bit", data => \@out);
    }
  }

  $out;
}

sub average_luminance {
  my ($class, $im) = @_;

  my @ch_avgs;
  for my $ch ( 0 .. $im->getchannels() - 1) {
    my @row_avgs;
    for my $y ( 0 .. $im->getheight() - 1) {
      my @samps = $im->getsamples(y => $y, scale => "linear", channels => [ $ch ], type => "float");
      push @row_avgs, _avg(@samps);
    }
    push @ch_avgs, _avg(@row_avgs);
  }

  \@ch_avgs;
}

sub _sum {
  my $sum = 0;
  for my $v (@_) {
    $sum += $v;
  }
  $sum;
}

sub _avg {
  _sum(@_) / @_;
}

1;

=for stopwords
ImageStuff

=head1 NAME

ImageStuff - perl implementations of various functions.

=head1 DESCRIPTION

This is mostly intended for adhoc testing and verification of Imager's
implementation.

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=cut
