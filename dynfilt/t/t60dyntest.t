#!perl -w
use strict;
use lib '../t';
use Test::More tests => 4;
BEGIN { use_ok(Imager => qw(:default)); }
use Config;

#$Imager::DEBUG=1;

Imager::init('log'=>'../testout/t60dyntest.log');

my $img=Imager->new() || die "unable to create image object\n";

$img->read(file=>'../testout/t104.ppm',type=>'pnm') 
  || die "failed: ",$img->{ERRSTR},"\n";

my $plug='./dyntest.'.$Config{'so'};
ok(load_plugin($plug), "load plugin")
  || die "unable to load plugin: $Imager::ERRSTR\n";

my %hsh=(a=>35,b=>200,type=>'lin_stretch');
ok($img->filter(%hsh), "call filter");

$img->write(type=>'pnm',file=>'../testout/t60.ppm') 
  || die "error in write()\n";

ok(unload_plugin($plug), "unload plugin")
  || die "unable to unload plugin: $Imager::ERRSTR\n";



