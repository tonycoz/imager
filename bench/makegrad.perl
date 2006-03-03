#!perl -w
use Imager;

=head1 NAME

makegrad.perl - builds a large gradient image for quant.c benchmarking

=head1 SYNOPSIS

  perl makegrad.perl

=cut

# a trans2 script to produce our pretty graduation
my $hsv = <<'EOS';
y cy - x cx - atan2 pi / 180 * !hue
1 1 x cx / y cy / distance !sat
@hue @sat 1 hsv
EOS

my $img = Imager::transform2({rpnexpr=>$hsv, width=>600, height=>600})
  or die "transform2 failed: $Imager::ERRSTR";

$img->write(file=>'hsvgrad.png', type=>'png')
  or die "Write to hsvgrad.png failed: ", $img->errstr;

# trans2 code to produce RGB tiles
my $rgb = <<'EOS';
8 !tilesper
w @tilesper / !tilex
h @tilesper / !tiley
x @tilex % @tilex / 255 * !red
y @tiley % @tiley / 255 * !green 
x @tilex / int 
y @tiley / int @tilesper * + @tilesper @tilesper * / 255 * !blue
@red @green @blue rgb
EOS
$img = Imager::transform2({rpnexpr=>$rgb, width=>600, height=>600})
  or die "transform2 failed: $Imager::ERRSTR";
$img->write(file=>'rgbtile.png', type=>'png')
  or die "write to rgbtile failed: ",$img->errstr;
