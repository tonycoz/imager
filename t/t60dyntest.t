BEGIN { $| = 1; print "1..3\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager qw(:default);
use Config;
$loaded = 1;

#$Imager::DEBUG=1;

Imager::init('log'=>'testout/t60dyntest.log');

$img=Imager->new() || die "unable to create image object\n";

$img->open(file=>'testout/t104.ppm',type=>'pnm') || die "failed: ",$img->{ERRSTR},"\n";

$plug='dynfilt/dyntest.'.$Config{'so'};
load_plugin($plug) || die "unable to load plugin:\n$Imager::ERRSTR\n";

print "ok\nok\n"; # exit;

%hsh=(a=>35,b=>200,type=>lin_stretch);
$img->filter(%hsh);

$img->write(type=>'pnm',file=>'testout/t60.ppm') || die "error in write()\n";

unload_plugin($plug) || die "unable to unload plugin: $Imager::ERRSTR\n";

print "ok\n";


