#!perl -w
use strict;
require "t/testtools.pl";
use Imager;

print "1..10\n";

#$Imager::DEBUG=1;

Imager::init('log'=>'testout/t65crop.log');

my $img=Imager->new() || die "unable to create image object\n";

okx($img, "created image ph");

if (okx($img->open(file=>'testimg/scale.ppm',type=>'pnm'), "loaded source")) {
  my $nimg = $img->crop(top=>10, left=>10, bottom=>25, right=>25);
  okx($nimg, "got an image");
  okx($nimg->write(file=>"testout/t65.ppm"), "save to file");
}
else {
  skipx(2, "couldn't load source image");
}

{ # https://rt.cpan.org/Ticket/Display.html?id=7578
  # make sure we get the right type of image on crop
  my $src = Imager->new(xsize=>50, ysize=>50, channels=>2, bits=>16);
  isx($src->getchannels, 2, "check src channels");
  isx($src->bits, 16, "check src bits");
  my $out = $src->crop(left=>10, right=>40, top=>10, bottom=>40);
  isx($out->getchannels, 2, "check out channels");
  isx($out->bits, 16, "check out bits");
}
{ # https://rt.cpan.org/Ticket/Display.html?id=7578
  print "# try it for paletted too\n";
  my $src = Imager->new(xsize=>50, ysize=>50, channels=>3, type=>'paletted');
  # make sure color index zero is defined so there's something to copy
  $src->addcolors(colors=>[Imager::Color->new(0,0,0)]);
  isx($src->type, 'paletted', "check source type");
  my $out = $src->crop(left=>10, right=>40, top=>10, bottom=>40);
  isx($out->type, 'paletted', 'check output type');
}
