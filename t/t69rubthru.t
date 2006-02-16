#!perl -w
use strict;
use Test::More tests => 28;
BEGIN { use_ok(Imager => qw(:all :handy)); }

init_log("testout/t69rubthru.log", 1);

my $src_height = 80;
my $src_width = 80;

# raw interface
my $targ = Imager::ImgRaw::new(100, 100, 3);
my $src = Imager::ImgRaw::new($src_height, $src_width, 4);
my $halfred = NC(255, 0, 0, 128);
i_box_filled($src, 20, 20, 60, 60, $halfred);
ok(i_rubthru($targ, $src, 10, 10, 0, 0, $src_width, $src_height),
   "low level rubthrough");
my $c = Imager::i_get_pixel($targ, 10, 10);
ok($c, "get pixel at (10, 10)");
ok(color_cmp($c, NC(0, 0, 0)) == 0, "check for correct color");
$c = Imager::i_get_pixel($targ, 30, 30);
ok($c, "get pixel at (30, 30)");
ok(color_cmp($c, NC(128, 0, 0)) == 0, "check color");

my $black = NC(0, 0, 0);
# reset the target and try a grey+alpha source
i_box_filled($targ, 0, 0, 100, 100, $black);
my $gsrc = Imager::ImgRaw::new($src_width, $src_height, 2);
my $halfwhite = NC(255, 128, 0);
i_box_filled($gsrc, 20, 20, 60, 60, $halfwhite);
ok(i_rubthru($targ, $gsrc, 10, 10, 0, 0, $src_width, $src_height),
   "low level with grey/alpha source");
$c = Imager::i_get_pixel($targ, 15, 15);
ok($c, "get at (15, 15)");
ok(color_cmp($c, NC(0, 0, 0)) == 0, "check color");
$c = Imager::i_get_pixel($targ, 30, 30);
ok($c, "get pixel at (30, 30)");
ok(color_cmp($c, NC(128, 128, 128)) == 0, "check color");

# try grey target and grey alpha source
my $gtarg = Imager::ImgRaw::new(100, 100, 1);
ok(i_rubthru($gtarg, $gsrc, 10, 10, 0, 0, $src_width, $src_height), 
   "low level with grey target and gray/alpha source");
$c = Imager::i_get_pixel($gtarg, 10, 10);
ok($c, "get pixel at 10, 10");
is(($c->rgba)[0], 0, "check grey level");
is((Imager::i_get_pixel($gtarg, 30, 30)->rgba)[0], 128,
   "check grey level at 30, 30");

# an attempt rub a 4 channel image over 1 channel should fail
ok(!i_rubthru($gtarg, $src, 10, 10, 0, 0, $src_width, $src_height),
   "check failure of 4 channel over 1 channel image");

# simple test for 16-bit/sample images
my $targ16 = Imager::i_img_16_new(100, 100, 3);
ok(i_rubthru($targ16, $src, 10, 10, 0, 0, $src_width, $src_height),
   "smoke test vs 16-bit/sample image");
$c = Imager::i_get_pixel($targ16, 30, 30);
ok($c, "get pixel at 30, 30");
ok(color_cmp($c, NC(128, 0, 0)) == 0, "check color");

# check the OO interface
my $ootarg = Imager->new(xsize=>100, ysize=>100);
my $oosrc = Imager->new(xsize=>80, ysize=>80, channels=>4);
$oosrc->box(color=>$halfred, xmin=>20, ymin=>20, xmax=>60, ymax=>60,
            filled=>1);
ok($ootarg->rubthrough(src=>$oosrc, tx=>10, ty=>10),
   "oo rubthrough");
ok(color_cmp(Imager::i_get_pixel($ootarg->{IMG}, 10, 10), NC(0, 0, 0)) == 0,
   "check pixel at 10, 10");
ok(color_cmp(Imager::i_get_pixel($ootarg->{IMG}, 30, 30), NC(128, 0, 0)) == 0,
   "check pixel at 30, 30");

# make sure we fail as expected
my $oogtarg = Imager->new(xsize=>100, ysize=>100, channels=>1);
ok(!$oogtarg->rubthrough(src=>$oosrc), "check oo fails correctly");

is($oogtarg->errstr, 
   'rubthru can only work where (dest, src) channels are (3,4), (3,2) or (1,2)',
   "check error message");

{ # check empty image errors
  my $empty = Imager->new;
  ok(!$empty->rubthrough(src => $oosrc), "check empty target");
  is($empty->errstr, 'empty input image', "check error message");
  ok(!$oogtarg->rubthrough(src=>$empty), "check empty source");
  is($oogtarg->errstr, 'empty input image for src',
     "check error message");
}

sub color_cmp {
  my ($l, $r) = @_;
  my @l = $l->rgba;
  my @r = $r->rgba;
  print "# (",join(",", @l[0..2]),") <=> (",join(",", @r[0..2]),")\n";
  return $l[0] <=> $r[0]
    || $l[1] <=> $r[1]
      || $l[2] <=> $r[2];
}


