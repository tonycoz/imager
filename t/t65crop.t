BEGIN { $| = 1; print "1..3\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;

$loaded = 1;

#$Imager::DEBUG=1;

Imager::init('log'=>'testout/t65crop.log');

$img=Imager->new() || die "unable to create image object\n";

print "ok 1\n";

$img->open(file=>'testimg/scale.ppm',type=>'pnm');

sub skip { 
    print $_[0];
    print "ok 2 # skip\n";
    print "ok 3 # skip\n";
    exit(0);
}


$nimg=$img->crop(top=>10, left=>10, bottom=>25, right=>25)
            or skip ( "\# warning ".$img->{'ERRSTR'}."\n" );

#	xopcodes=>[qw( x y Add)],yopcodes=>[qw( x y Sub)],parm=>[]

print "ok 2\n";
$nimg->write(type=>'pnm',file=>'testout/t65.ppm') || die "error in write()\n";

print "ok 3\n";
