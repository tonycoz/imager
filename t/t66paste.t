BEGIN { $| = 1; print "1..4\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;

$loaded = 1;

#$Imager::DEBUG=1;

Imager::init('log'=>'testout/t66paste.log');

$img=Imager->new() || die "unable to create image object\n";

print "ok 1\n";

$img->open(file=>'testimg/scale.ppm',type=>'pnm');

$nimg=Imager->new() or die "Unable to create image object\n";
$nimg->open(file=>'testimg/scale.ppm',type=>'pnm');
print "ok 2\n";

$img->paste(img=>$nimg, top=>30, left=>30) or die $img->{ERRSTR};
print "ok 3\n";


$img->write(type=>'pnm',file=>'testout/t66.ppm') || die "error in write()\n";
print "ok 4\n";


