# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..2\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;

$loaded = 1;

print "ok 1\n";

Imager::init("log"=>"testout/t50basicoo.log");

%hsh=%Imager::formats;

print "# avaliable formats:\n";
for(keys %hsh) { print "# $_\n"; }

#print Dumper(\%hsh);

$img = Imager->new();

@types = qw( jpeg png raw ppm gif tiff );

@files{@types} = ({ file => "testout/t101.jpg"  },
		  { file => "testout/t102.png"  },
		  { file => "testout/t103.raw", xsize=>150, ysize=>150, type=>"raw" },
		  { file => "testout/t104.ppm"  },
		  { file => "testout/t105.gif"  },
		  { file => "testout/t106.tiff" });

for $type (@types) {
  next unless $hsh{$type};
  my %opts = %{$files{$type}};
  my @a = map { "$_=>${opts{$_}}" } keys %opts;
  print "#opening Format: $type, options: @a\n";
  $img->read( %opts ) or die "failed: ",$img->errstr,"\n";
}

$img2 =  $img->crop(width=>50, height=>50);
$img2 -> write(file=> 'testout/t50.ppm', type=>'pnm');

undef($img);

Imager::malloc_state();

print "ok 2\n";
