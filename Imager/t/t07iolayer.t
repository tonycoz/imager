BEGIN { $|=1; print "1..7\n"; }
END { print "not ok 1\n" unless $loaded; };
use Imager qw(:all);
++$loaded;
print "ok 1\n";
init_log("testout/t07iolayer.log", 1);


undef($/);
# start by testing io buffer

$data="P2\n2 2\n255\n 255 0\n0 255\n";
$IO = Imager::io_new_buffer($data);
$im = Imager::i_readpnm_wiol($IO, -1);

print "ok 2\n";


open(FH, ">testout/t07.ppm") or die $!;
binmode(FH);
$fd = fileno(FH);
$IO2 = Imager::io_new_fd( $fd );
Imager::i_writeppm_wiol($im, $IO2);
close(FH);
undef($im);



open(FH, "<testimg/penguin-base.ppm");
binmode(FH);
$data = <FH>;
close(FH);
$IO3 = Imager::io_new_buffer($data);
#undef($data);
$im = Imager::i_readpnm_wiol($IO3, -1);

print "ok 3\n";


open(FH, "<testimg/penguin-base.ppm") or die $!;
binmode(FH);
$fd = fileno(FH);
$IO4 = Imager::io_new_fd( $fd );
$im2 = Imager::i_readpnm_wiol($IO4, -1);
close(FH);
undef($IO4);

print "ok 4\n";

Imager::i_img_diff($im, $im2) ? print "not ok 5\n" : print "ok 5\n";
undef($im2);


$IO5 = Imager::io_new_bufchain();
Imager::i_writeppm_wiol($im, $IO5);
$data2 = Imager::io_slurp($IO5);
undef($IO5);

print "ok 6\n";

$IO6 = Imager::io_new_buffer($data2);
$im3 = Imager::i_readpnm_wiol($IO6, -1);

Imager::i_img_diff($im, $im3) ? print "not ok 7\n" : print "ok 7\n";




