#!perl -w
use strict;
use lib 't';
use Test::More tests => 16;

BEGIN { use_ok(Imager=>':all') }

require "t/testtools.pl";

Imager::init('log'=>'testout/t40scale.log');
my $img=Imager->new();

ok($img->open(file=>'testimg/scale.ppm',type=>'pnm'),
   "load test image") or print "# ",$img->errstr,"\n";

my $scaleimg=$img->scale(scalefactor=>0.25)
  or print "# ",$img->errstr,"\n";
ok($scaleimg, "scale it (good mode)");

ok($scaleimg->write(file=>'testout/t40scale1.ppm',type=>'pnm'),
   "save scaled image") or print "# ",$img->errstr,"\n";

$scaleimg=$img->scale(scalefactor=>0.25,qtype=>'preview');
ok($scaleimg, "scale it (preview)") or print "# ",$img->errstr,"\n";

ok($scaleimg->write(file=>'testout/t40scale2.ppm',type=>'pnm'),
   "write preview scaled image")  or print "# ",$img->errstr,"\n";

{
  # check for a warning when scale() is called in void context
  my $warning;
  local $SIG{__WARN__} = 
    sub { 
      $warning = "@_";
      my $printed = $warning;
      $printed =~ s/\n$//;
      $printed =~ s/\n/\n\#/g; 
      print "# ",$printed, "\n";
    };
  $img->scale(scalefactor=>0.25);
  cmp_ok($warning, '=~', qr/void/, "check warning");
  cmp_ok($warning, '=~', qr/t40scale\.t/, "check filename");
  $warning = '';
  $img->scaleX(scalefactor=>0.25);
  cmp_ok($warning, '=~', qr/void/, "check warning");
  cmp_ok($warning, '=~', qr/t40scale\.t/, "check filename");
  $warning = '';
  $img->scaleY(scalefactor=>0.25);
  cmp_ok($warning, '=~', qr/void/, "check warning");
  cmp_ok($warning, '=~', qr/t40scale\.t/, "check filename");
}
{ # https://rt.cpan.org/Ticket/Display.html?id=7467
  # segfault in Imager 0.43
  # make sure scale() doesn't let us make an image zero pixels high or wide
  # it does this by making the given axis as least 1 pixel high
  my $out = $img->scale(scalefactor=>0.00001);
  is($out->getwidth, 1, "min scale width");
  is($out->getheight, 1, "min scale height");

  $out = $img->scale(scalefactor=>0.00001, qtype => 'preview');
  is($out->getwidth, 1, "min scale width (preview)");
  is($out->getheight, 1, "min scale height (preview)");
}
