# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..22\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;
$loaded = 1;
print "ok 1\n";

init_log("testout/t15color.log",1);

my $c1 = Imager::Color->new(100, 150, 200, 250);
print test_col($c1, 100, 150, 200, 250) ? "ok 2\n" : "not ok 2\n";
my $c2 = Imager::Color->new(100, 150, 200);
print test_col($c2, 100, 150, 200, 255) ? "ok 3\n" : "not ok 3\n";
my $c3 = Imager::Color->new("#6496C8");
print test_col($c3, 100, 150, 200, 255) ? "ok 4\n" : "not ok 4\n";
# crashes in Imager-0.38pre8 and earlier
my @foo;
for (1..1000) {
  push(@foo, Imager::Color->new("#FFFFFF"));
}
my $fail;
for (@foo) {
  Imager::Color::set_internal($_, 128, 128, 128, 128) == $_ or ++$fail;
  Imager::Color::set_internal($_, 128, 128, 128, 128) == $_ or ++$fail;
  test_col($_, 128, 128, 128, 128) or ++$fail;
}
$fail and print "not ";
print "ok 5\n";

# test the new OO methods
color_ok(6, 100, 150, 200, 255, Imager::Color->new(r=>100, g=>150, b=>200));
color_ok(7, 101, 151, 201, 255, 
         Imager::Color->new(red=>101, green=>151, blue=>201));
color_ok(8, 102, 255, 255, 255, Imager::Color->new(grey=>102));
color_ok(9, 103, 255, 255, 255, Imager::Color->new(gray=>103));
if (-e '/usr/lib/X11/rgb.txt') {
  color_ok(10, 0, 0, 255, 255, Imager::Color->new(xname=>'blue'));
}
else {
  print "ok 10 # skip - no X rgb.txt found\n";
}
color_ok(11, 255, 250, 250, 255, 
         Imager::Color->new(gimp=>'snow', palette=>'testimg/test_gimp_pal'));
color_ok(12, 255, 255, 255, 255, Imager::Color->new(h=>0, 's'=>0, 'v'=>1.0));
color_ok(13, 255, 0, 0, 255, Imager::Color->new(h=>0, 's'=>1, v=>1));
color_ok(14, 128, 129, 130, 255, Imager::Color->new(web=>'#808182'));
color_ok(15, 0x11, 0x22, 0x33, 255, Imager::Color->new(web=>'#123'));
color_ok(16, 255, 150, 121, 255, Imager::Color->new(rgb=>[ 255, 150, 121 ]));
color_ok(17, 255, 150, 121, 128, 
         Imager::Color->new(rgba=>[ 255, 150, 121, 128 ]));
color_ok(18, 255, 0, 0, 255, Imager::Color->new(hsv=>[ 0, 1, 1 ]));
color_ok(19, 129, 130, 131, 134, 
         Imager::Color->new(channel0=>129, channel1=>130, channel2=>131,
                            channel3=>134));
color_ok(20, 129, 130, 131, 134, 
         Imager::Color->new(c0=>129, c1=>130, c2=>131, c3=>134));
color_ok(21, 200, 201, 203, 204, 
         Imager::Color->new(channels=>[ 200, 201, 203, 204 ]));
color_ok(22, 255, 250, 250, 255, 
         Imager::Color->new(name=>'snow', palette=>'testimg/test_gimp_pal'));
sub test_col {
  my ($c, $r, $g, $b, $a) = @_;
  unless ($c) {
    print "# $Imager::ERRSTR\n";
    return 0;
  }
  my ($cr, $cg, $cb, $ca) = $c->rgba;
  return $r == $cr && $g == $cg && $b == $cb && $a == $ca;
}

sub color_ok {
  my ($test_num, $r, $g, $b, $a, $c) = @_;

  if (test_col($c, $r, $g, $b, $a)) {
    print "ok $test_num\n";
  }
  else {
    print "not ok $test_num\n"
  }
}

