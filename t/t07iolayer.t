BEGIN { $|=1; print "1..20\n"; }
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
undef $IO3;

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

ok(7, Imager::i_img_diff($im, $im3) == 0, "read from buffer");

my $work = $data;
my $pos = 0;
sub io_reader {
  my ($size, $maxread) = @_;
  my $out = substr($work, $pos, $maxread);
  $pos += length $out;
  $out;
}
sub io_reader2 {
  my ($size, $maxread) = @_;
  my $out = substr($work, $pos, $maxread);
  $pos += length $out;
  $out;
}
my $IO7 = Imager::io_new_cb(undef, \&io_reader, undef, undef);
ok(8, $IO7, "making readcb object");
my $im4 = Imager::i_readpnm_wiol($IO7, -1);
ok(9, $im4, "read from cb");
ok(10, Imager::i_img_diff($im, $im4) == 0, "read from cb image match");

$pos = 0;
$IO7 = Imager::io_new_cb(undef, \&io_reader2, undef, undef);
ok(11, $IO7, "making short readcb object");
my $im5 = Imager::i_readpnm_wiol($IO7, -1);
ok(12, $im4, "read from cb2");
ok(13, Imager::i_img_diff($im, $im5) == 0, "read from cb2 image match");

sub io_writer {
  my ($what) = @_;
  substr($work, $pos, $pos+length $what) = $what;
  $pos += length $what;

  1;
}

my $did_close;
sub io_close {
  ++$did_close;
}

my $IO8 = Imager::io_new_cb(\&io_writer, undef, undef, \&io_close);
ok(14, $IO8, "making writecb object");
$pos = 0;
$work = '';
ok(15, Imager::i_writeppm_wiol($im, $IO8), "write to cb");
# I originally compared this to $data, but that doesn't include the
# Imager header
ok(16, $work eq $data2, "write image match");
ok(17, $did_close, "did close");

# with a short buffer, no closer
my $IO9 = Imager::io_new_cb(\&io_writer, undef, undef, undef, 1);
ok(18, $IO9, "making short writecb object");
$pos = 0;
$work = '';
ok(19, Imager::i_writeppm_wiol($im, $IO9), "write to short cb");
ok(20, $work eq $data2, "short write image match");

sub ok {
  my ($num, $ok, $what) = @_;

  if ($ok) {
    print "ok $num # $what\n";
  }
  else {
    print "not ok $num # $what\n";
  }
  $ok;
}
