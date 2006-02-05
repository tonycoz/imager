#!perl -w
use strict;
use lib 't';
use Test::More tests => 46;

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

{ # error handling - NULL image
  my $im = Imager->new;
  ok(!$im->scale(scalefactor => 0.5), "try to scale empty image");
  is($im->errstr, "empty input image", "check error message");
}

{ # invalid qtype value
  my $im = Imager->new(xsize => 100, ysize => 100);
  ok(!$im->scale(scalefactor => 0.5, qtype=>'unknown'), "unknown qtype");
  is($im->errstr, "invalid value for qtype parameter", "check error message");
  
  # invalid type value
  ok(!$im->scale(xpixels => 10, ypixels=>50, type=>"unknown"), "unknown type");
  is($im->errstr, "invalid value for type parameter", "check error message");
}

SKIP:
{ # Image::Math::Constrain support
  eval "require Image::Math::Constrain;";
  $@ and skip "module optional Image::Math::Constrain not installed", 3;
  my $constrain = Image::Math::Constrain->new(20, 100);
  my $im = Imager->new(xsize => 160, ysize => 96);
  my $result = $im->scale(constrain => $constrain);
  ok($result, "successful scale with Image::Math::Constrain");
  is($result->getwidth, 20, "check result width");
  is($result->getheight, 12, "check result height");
}

{ # scale size checks
  my $im = Imager->new(xsize => 160, ysize => 96); # some random size

  scale_test($im, 80, 48, "48 x 48 def type",
	     xpixels => 48, ypixels => 48);
  scale_test($im, 80, 48, "48 x 48 max type",
	     xpixels => 48, ypixels => 48, type => 'max');
  scale_test($im, 80, 48, "80 x 80 min type",
	     xpixels => 80, ypixels => 80, type => 'min');
  scale_test($im, 80, 48, "no scale parameters (default to 0.5 scalefactor)");
  scale_test($im, 120, 72, "0.75 scalefactor",
	     scalefactor => 0.75);
  scale_test($im, 80, 48, "80 width",
	     xpixels => 80);
  scale_test($im, 120, 72, "72 height",
	     ypixels => 72);
}

sub scale_test {
  my ($in, $exp_width, $exp_height, $note, @parms) = @_;

  print "# $note: @parms\n";
 SKIP:
  {
    my $scaled = $in->scale(@parms);
    ok($scaled, "scale $note")
      or skip("failed to scale", 2);
    is($scaled->getwidth, $exp_width, "check width");
    is($scaled->getheight, $exp_height, "check height");
  }
}
