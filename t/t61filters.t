#!perl -w
use strict;
use Imager qw(:handy);

Imager::init_log("testout/t61filters.log", 1);
# meant for testing the filters themselves
my $imbase = Imager->new;
$imbase->open(file=>'testout/t104.ppm') or die;
my $im_other = Imager->new(xsize=>150, ysize=>150);
$im_other->box(xmin=>30, ymin=>60, xmax=>120, ymax=>90, filled=>1);

print "1..45\n";

test($imbase, 1, {type=>'autolevels'}, 'testout/t61_autolev.ppm');

test($imbase, 3, {type=>'contrast', intensity=>0.5}, 
     'testout/t61_contrast.ppm');

# this one's kind of cool
test($imbase, 5, {type=>'conv', coef=>[ -0.5, 1, -0.5, ], },
     'testout/t61_conv.ppm');

test($imbase, 7, {type=>'gaussian', stddev=>5 },
     'testout/t61_gaussian.ppm');

test($imbase, 9, { type=>'gradgen', dist=>1,
                   xo=>[ 10,  10, 120 ],
                   yo=>[ 10, 140,  60 ],
                   colors=> [ NC('#FF0000'), NC('#FFFF00'), NC('#00FFFF') ]},
     'testout/t61_gradgen.ppm');

test($imbase, 11, {type=>'mosaic', size=>8}, 'testout/t61_mosaic.ppm');

test($imbase, 13, {type=>'hardinvert'}, 'testout/t61_hardinvert.ppm');

test($imbase, 15, {type=>'noise'}, 'testout/t61_noise.ppm');

test($imbase, 17, {type=>'radnoise'}, 'testout/t61_radnoise.ppm');

test($imbase, 19, {type=>'turbnoise'}, 'testout/t61_turbnoise.ppm');

test($imbase, 21, {type=>'bumpmap', bump=>$im_other, lightx=>30, lighty=>30},
     'testout/t61_bumpmap.ppm');

test($imbase, 23, {type=>'bumpmap_complex', bump=>$im_other}, 'testout/t61_bumpmap_complex.ppm');

test($imbase, 25, {type=>'postlevels', levels=>3}, 'testout/t61_postlevels.ppm');

test($imbase, 27, {type=>'watermark', wmark=>$im_other },
     'testout/t61_watermark.ppm');

test($imbase, 29, {type=>'fountain', xa=>75, ya=>75, xb=>85, yb=>30,
                   repeat=>'triangle', #ftype=>'radial', 
                   super_sample=>'circle', ssample_param => 16,
                  },
     'testout/t61_fountain.ppm');
use Imager::Fountain;

my $f1 = Imager::Fountain->new;
$f1->add(end=>0.2, c0=>NC(255, 0,0), c1=>NC(255, 255,0));
$f1->add(start=>0.2, c0=>NC(255,255,0), c1=>NC(0,0,255,0));
test($imbase, 31, { type=>'fountain', xa=>20, ya=>130, xb=>130, yb=>20,
                    #repeat=>'triangle',
                    segments=>$f1
                  },
     'testout/t61_fountain2.ppm');
my $f2 = Imager::Fountain->new
  ->add(end=>0.5, c0=>NC(255,0,0), c1=>NC(255,0,0), color=>'hueup')
  ->add(start=>0.5, c0=>NC(255,0,0), c1=>NC(255,0,0), color=>'huedown');
#use Data::Dumper;
#print Dumper($f2);
test($imbase, 33, { type=>'fountain', xa=>20, ya=>130, xb=>130, yb=>20,
                    segments=>$f2 },
     'testout/t61_fount_hsv.ppm');
my $f3 = Imager::Fountain->read(gimp=>'testimg/gimpgrad')
  or print "not ";
print "ok 35\n";
test($imbase, 36, { type=>'fountain', xa=>75, ya=>75, xb=>90, yb=>15,
                    segments=>$f3, super_sample=>'grid',
                    ftype=>'radial_square', combine=>'color' },
     'testout/t61_fount_gimp.ppm');
test($imbase, 38, { type=>'unsharpmask', stddev=>2.0 },
     'testout/t61_unsharp.ppm');
test($imbase, 40, {type=>'conv', coef=>[ -1, 3, -1, ], },
     'testout/t61_conv_sharp.ppm');

# Regression test: the checking of the segment type was incorrect
# (the comparison was checking the wrong variable against the wrong value)
my $f4 = [ [ 0, 0.5, 1, NC(0,0,0), NC(255,255,255), 5, 0 ] ];
test($imbase, 42, {type=>'fountain',  xa=>75, ya=>75, xb=>90, yb=>15,
                    segments=>$f4, super_sample=>'grid',
                    ftype=>'linear', combine=>'color' },
     'testout/t61_regress_fount.ppm');
my $im2 = $imbase->copy;
$im2->box(xmin=>20, ymin=>20, xmax=>40, ymax=>40, color=>'FF0000', filled=>1);
$im2->write(file=>'testout/t61_diff_base.ppm');
my $im3 = Imager->new(xsize=>150, ysize=>150, channels=>3);
$im3->box(xmin=>20, ymin=>20, xmax=>40, ymax=>40, color=>'FF0000', filled=>1);
my $diff = $imbase->difference(other=>$im2);
print $diff ? "ok 44\n" : "not ok 44\n";
if ($im3 && $diff) {
  print Imager::i_img_diff($im3->{IMG}, $diff->{IMG}) ? "not ok 45\n" : "ok 45\n";
}
else {
  print "ok 45 # skip\n";
}

sub test {
  my ($in, $num, $params, $out) = @_;

  my $copy = $in->copy;
  if ($copy->filter(%$params)) {
    print "ok $num\n";
    if ($copy->write(file=>$out)) {
      print "ok ",$num+1,"\n";
    }
    else {
      print "not ok ",$num+1," # ",$copy->errstr,"\n";
    }
  }
  else {
    print "not ok $num # ",$copy->errstr,"\n";
    print "ok ",$num+1," # skipped\n";
  }
}
