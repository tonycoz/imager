#!perl -w

BEGIN { $| = 1; print "1..23\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager qw(:all :handy);
$loaded = 1;
print "ok 1\n";
init_log("testout/t69rubthru.log", 1);

# raw interface
my $targ = Imager::ImgRaw::new(100, 100, 3);
my $src = Imager::ImgRaw::new(80, 80, 4);
my $halfred = NC(255, 0, 0, 128);
i_box_filled($src, 20, 20, 60, 60, $halfred);
i_rubthru($targ, $src, 10, 10) or print "not ";
print "ok 2\n";
my $c = Imager::i_get_pixel($targ, 10, 10) or print "not ";
print "ok 3\n";
color_cmp($c, NC(0, 0, 0)) == 0 or print "not ";
print "ok 4\n";
$c = Imager::i_get_pixel($targ, 30, 30) or print "not ";
print "ok 5\n";
color_cmp($c, NC(128, 0, 0)) == 0 or print "not ";
print "ok 6\n";

my $black = NC(0, 0, 0);
# reset the target and try a grey+alpha source
i_box_filled($targ, 0, 0, 100, 100, $black);
my $gsrc = Imager::ImgRaw::new(80, 80, 2);
my $halfwhite = NC(255, 128, 0);
i_box_filled($gsrc, 20, 20, 60, 60, $halfwhite);
i_rubthru($targ, $gsrc, 10, 10) or print "not ";
print "ok 7\n";
$c = Imager::i_get_pixel($targ, 15, 15) or print "not ";
print "ok 8\n";
color_cmp($c, NC(0, 0, 0)) == 0 or print "not ";
print "ok 9\n";
$c = Imager::i_get_pixel($targ, 30, 30) or print "not ";
print "ok 10\n";
color_cmp($c, NC(128, 128, 128)) == 0 or print "not ";
print "ok 11\n";

# try grey target and grey alpha source
my $gtarg = Imager::ImgRaw::new(100, 100, 1);
i_rubthru($gtarg, $gsrc, 10, 10) or print "not ";
print "ok 12\n";
$c = Imager::i_get_pixel($gtarg, 10, 10) or print "not ";
print "ok 13\n";
($c->rgba)[0] == 0 or print "not ";
print "ok 14\n";
(Imager::i_get_pixel($gtarg, 30, 30)->rgba)[0] == 128 or print "not ";
print "ok 15\n";

# an attempt rub a 4 channel image over 1 channel should fail
i_rubthru($gtarg, $src, 10, 10) and print "not ";
print "ok 16\n";

# simple test for 16-bit/sample images
my $targ16 = Imager::i_img_16_new(100, 100, 3);
i_rubthru($targ16, $src, 10, 10) or print "not ";
print "ok 17\n";
$c = Imager::i_get_pixel($targ16, 30, 30) or print "not ";
print "ok 18\n";
color_cmp($c, NC(128, 0, 0)) == 0 or print "not ";
print "ok 19\n";

# check the OO interface
my $ootarg = Imager->new(xsize=>100, ysize=>100);
my $oosrc = Imager->new(xsize=>80, ysize=>80, channels=>4);
$oosrc->box(color=>$halfred, xmin=>20, ymin=>20, xmax=>60, ymax=>60, 
            filled=>1);
$ootarg->rubthrough(src=>$oosrc, tx=>10, ty=>10) or print "not ";
print "ok 20\n";
color_cmp(Imager::i_get_pixel($ootarg->{IMG}, 10, 10), NC(0, 0, 0)) == 0
  or print "not ";
print "ok 21\n";
color_cmp(Imager::i_get_pixel($ootarg->{IMG}, 30, 30), NC(128, 0, 0)) == 0
  or print "not ";
print "ok 22\n";

# make sure we fail as expected
my $oogtarg = Imager->new(xsize=>100, ysize=>100, channels=>1);
$oogtarg->rubthrough(src=>$oosrc) and print "not ";
print "ok 23\n";


sub color_cmp {
  my ($l, $r) = @_;
  my @l = $l->rgba;
  my @r = $r->rgba;
  print "# (",join(",", @l[0..2]),") <=> (",join(",", @r[0..2]),")\n";
  return $l[0] <=> $r[0]
    || $l[1] <=> $r[1]
      || $l[2] <=> $r[2];
}


