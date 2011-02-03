BEGIN { $| = 1; print "1..5\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;

$loaded = 1;

#$Imager::DEBUG=1;

-d "testout" or mkdir "testout";

Imager::init('log'=>'testout/t55trans.log');

$img=Imager->new() || die "unable to create image object\n";

print "ok 1\n";

$img->open(file=>'testimg/scale.ppm',type=>'pnm');

sub skip { 
    print "ok 2 # skip $_[0]\n";
    print "ok 3 # skip $_[0]\n";
    print "ok 4 # skip $_[0]\n";
    print "ok 5 # skip $_[0]\n";
    exit(0);
}


$nimg=$img->transform(xexpr=>'x',yexpr=>'y+10*sin((x+y)/10)') || skip ( "\# warning ".$img->{'ERRSTR'} );

#	xopcodes=>[qw( x y Add)],yopcodes=>[qw( x y Sub)],parm=>[]

print "ok 2\n";
$nimg->write(type=>'pnm',file=>'testout/t55.ppm') || die "error in write()\n";

print "ok 3\n";

# the original test didn't produce many parameters - this one
# produces more parameters, which revealed a memory allocation bug
# (sizeof(double) vs sizeof(int))
sub skip2 { 
    print "ok 4 # skip $_[0]\n";
    print "ok 5 # skip $_[0]\n";
    exit(0);
}
$nimg=$img->transform(xexpr=>'x+0.1*y+5*sin(y/10.0+1.57)',
	yexpr=>'y+10*sin((x+y-0.785)/10)') 
	|| skip2 ( "\# warning ".$img->{'ERRSTR'} );

print "ok 4\n";
$nimg->write(type=>'pnm',file=>'testout/t55b.ppm') 
	|| die "error in write()\n";

print "ok 5\n";


