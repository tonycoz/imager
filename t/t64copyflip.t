BEGIN { $| = 1; print "1..5\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;

$loaded = 1;

#$Imager::DEBUG=1;

Imager::init('log'=>'testout/t64copyflip.log');

$img=Imager->new() or die "unable to create image object\n";

$img->open(file=>'testimg/scale.ppm',type=>'pnm');
$nimg = $img->copy() or skip ( "\# warning ".$img->{'ERRSTR'}."\n" );
print "ok 1\n";

# test if ->copy() works

$diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
if ($diff > 0) { print "not ok 2 # copy and original differ!\n"; }
else { print "ok 2\n"; }


# test if ->flip(dir=>'h')->flip(dir=>'h') doesn't alter the image

$nimg->flip(dir=>"h")->flip(dir=>"h");
$diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
if ($diff > 0) { print "not ok 3 # double horizontal flip and original differ!\n"; }
else { print "ok 3\n"; }


# test if ->flip(dir=>'v')->flip(dir=>'v') doesn't alter the image

$nimg->flip(dir=>"v")->flip(dir=>"v");
$diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
if ($diff > 0) { print "not ok 4 # double vertical flip and original differ!\n"; }
else { print "ok 4\n"; }


# test if ->flip(dir=>'h')->flip(dir=>'v') is same as ->flip(dir=>'hv')

$nimg->flip(dir=>"v")->flip(dir=>"h")->flip(dir=>"hv");;
$diff = Imager::i_img_diff($img->{IMG}, $nimg->{IMG});
if ($diff > 0) {
  print "not ok 5 # double flips and original differ!\n";
  $nimg->write(file=>"testout/t64copyflip_error.ppm") or die $nimg->errstr();
}
else { print "ok 5\n"; }

sub skip {
    print $_[0];
    print "ok 2 # skip\n";
    print "ok 3 # skip\n";
    print "ok 4 # skip\n";
    print "ok 5 # skip\n";
    exit(0);
}

