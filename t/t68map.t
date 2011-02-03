-d "testout" or mkdir "testout";

Imager::init("log"=>'testout/t68map.log');

use Imager qw(:all :handy);

print "1..5\n";

my $imbase = Imager::ImgRaw::new(200,300,3);


@map1 = map { int($_/2) } 0..255;
@map2 = map { 255-int($_/2) } 0..255;
@map3 = 0..255;
@maps = 0..24;
@mapl = 0..400;

$tst = 1;

i_map($imbase, [ [],     [],     \@map1 ]);
print "ok ".$tst++."\n";
i_map($imbase, [ \@map1, \@map1, \@map1 ]);

print "ok ".$tst++."\n";
i_map($imbase, [ \@map1, \@map2, \@map3 ]);

print "ok ".$tst++."\n";
i_map($imbase, [ \@maps, \@mapl, \@map3 ]);

# test the highlevel interface
# currently this requires visual inspection of the output files


my $im = Imager->new;
if ($im->read(file=>'testimg/scale.ppm')) {
  print( ( $im->map(red=>\@map1, green=>\@map2, blue=>\@map3) ? "ok " : "not ok ").$tst++."\n" );
  print( ( $im->map(maps=>[\@map1, [], \@map2]) ? "ok " : "not ok "). $tst++."\n");
}
else {
  die "could not load testout/scale.ppm\n";
}
