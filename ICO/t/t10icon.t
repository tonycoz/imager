#!perl -w
use strict;
use Test::More tests => 60;

BEGIN { use_ok('Imager::File::ICO'); }

-d 'testout' or mkdir 'testout';

my $im = Imager->new;
# type=>'ico' or 'cur' and read ico and cur since they're pretty much
# the same
ok($im->read(file => "testimg/rgba3232.ico", type=>"ico"),
   "read 32 bit")
  or print "# ", $im->errstr, "\n";
is($im->getwidth, 32, "check width");
is($im->getwidth, 32, "check height");
is($im->type, 'direct', "check type");
is($im->tags(name => 'ico_bits'), 32, "check ico_bits tag");
is($im->tags(name => 'i_format'), 'ico', "check i_format tag");
my $mask = '.*
..........................******
..........................******
..........................******
..........................******
...........................*****
............................****
............................****
.............................***
.............................***
.............................***
.............................***
..............................**
..............................**
...............................*
...............................*
................................
................................
................................
................................
................................
................................
*...............................
**..............................
**..............................
***.............................
***.............................
****............................
****............................
*****...........................
*****...........................
*****...........................
*****...........................';
is($im->tags(name => 'ico_mask'), $mask, "check ico_mask_tag");

# compare the pixels
# ppm can't store 4 channels
SKIP:
{
  my $work = $im->convert(preset=>'noalpha');
  my $comp = Imager->new;
  $comp->read(file => "testimg/rgba3232.ppm")
    or skip "could not read 24-bit comparison file:". $comp->errstr, 1;
  is(Imager::i_img_diff($comp->{IMG}, $work->{IMG}), 0,
     "compare image data");
}

ok($im->read(file => 'testimg/pal83232.ico', type=>'ico'),
   "read 8 bit")
  or print "# ", $im->errstr, "\n";
is($im->getwidth, 32, "check width");
is($im->getwidth, 32, "check height");
is($im->type, 'paletted', "check type");
is($im->colorcount, 256, "color count");
is($im->tags(name => 'ico_bits'), 8, "check ico_bits tag");
is($im->tags(name => 'i_format'), 'ico', "check i_format tag");
SKIP:
{
  my $comp = Imager->new;
  $comp->read(file => "testimg/pal83232.ppm")
    or skip "could not read 8-bit comparison file:". $comp->errstr, 1;
  is(Imager::i_img_diff($comp->{IMG}, $im->{IMG}), 0,
     "compare image data");
}
$im->write(file=>'testout/pal83232.ppm');

ok($im->read(file => 'testimg/pal43232.ico', type=>'ico'),
   "read 4 bit")
  or print "# ", $im->errstr, "\n";
is($im->getwidth, 32, "check width");
is($im->getwidth, 32, "check height");
is($im->type, 'paletted', "check type");
is($im->colorcount, 16, "color count");
is($im->tags(name => 'ico_bits'), 4, "check ico_bits tag");
is($im->tags(name => 'i_format'), 'ico', "check i_format tag");
SKIP:
{
  my $comp = Imager->new;
  $comp->read(file => "testimg/pal43232.ppm")
    or skip "could not read 4-bit comparison file:". $comp->errstr, 1;
  is(Imager::i_img_diff($comp->{IMG}, $im->{IMG}), 0,
     "compare image data");
}

$im->write(file=>'testout/pal43232.ppm');
ok($im->read(file => 'testimg/pal13232.ico', type=>'ico'),
   "read 1 bit")
  or print "# ", $im->errstr, "\n";
is($im->getwidth, 32, "check width");
is($im->getwidth, 32, "check height");
is($im->type, 'paletted', "check type");
is($im->colorcount, 2, "color count");
is($im->tags(name => 'cur_bits'), 1, "check ico_bits tag");
is($im->tags(name => 'i_format'), 'cur', "check i_format tag");
$im->write(file=>'testout/pal13232.ppm');

# combo was created with the GIMP, which has a decent mechanism for selecting
# the output format
# you get different size icon images by making different size layers.
my @imgs = Imager->read_multi(file => 'testimg/combo.ico', type=>'ico');
is(scalar(@imgs), 3, "read multiple");
is($imgs[0]->getwidth, 48, "image 0 width");
is($imgs[0]->getheight, 48, "image 0 height");
is($imgs[1]->getwidth, 32, "image 1 width");
is($imgs[1]->getheight, 32, "image 1 height");
is($imgs[2]->getwidth, 16, "image 2 width");
is($imgs[2]->getheight, 16, "image 2 height");
is($imgs[0]->type, 'direct', "image 0 type");
is($imgs[1]->type, 'paletted', "image 1 type");
is($imgs[2]->type, 'paletted', "image 2 type");
is($imgs[1]->colorcount, 256, "image 1 colorcount");
is($imgs[2]->colorcount, 16, "image 2 colorcount");

is_deeply([ $imgs[0]->getpixel(x=>0, 'y'=>0)->rgba ], [ 231, 17, 67, 255 ],
	  "check image data 0(0,0)");
is_deeply([ $imgs[1]->getpixel(x=>0, 'y'=>0)->rgba ], [ 231, 17, 67, 255 ],
	  "check image data 1(0,0)");
is_deeply([ $imgs[2]->getpixel(x=>0, 'y'=>0)->rgba ], [ 231, 17, 67, 255 ],
	  "check image data 2(0,0)");

is_deeply([ $imgs[0]->getpixel(x=>47, 'y'=>0)->rgba ], [ 131, 231, 17, 255 ],
	  "check image data 0(47,0)");
is_deeply([ $imgs[1]->getpixel(x=>31, 'y'=>0)->rgba ], [ 131, 231, 17, 255 ],
	  "check image data 1(31,0)");
is_deeply([ $imgs[2]->getpixel(x=>15, 'y'=>0)->rgba ], [ 131, 231, 17, 255 ],
	  "check image data 2(15,0)");

is_deeply([ $imgs[0]->getpixel(x=>0, 'y'=>47)->rgba ], [ 17, 42, 231, 255 ],
	  "check image data 0(0,47)");
is_deeply([ $imgs[1]->getpixel(x=>0, 'y'=>31)->rgba ], [ 17, 42, 231, 255 ],
	  "check image data 1(0,31)");
is_deeply([ $imgs[2]->getpixel(x=>0, 'y'=>15)->rgba ], [ 17, 42, 231, 255 ],
	  "check image data 2(0,15)");

is_deeply([ $imgs[0]->getpixel(x=>47, 'y'=>47)->rgba ], [ 17, 231, 177, 255 ],
	  "check image data 0(47,47)");
is_deeply([ $imgs[1]->getpixel(x=>31, 'y'=>31)->rgba ], [ 17, 231, 177, 255 ],
	  "check image data 1(31,31)");
is_deeply([ $imgs[2]->getpixel(x=>15, 'y'=>15)->rgba ], [ 17, 231, 177, 255 ],
	  "check image data 2(15,15)");

$im = Imager->new(xsize=>32, ysize=>32);
$im->box(filled=>1, color=>'FF0000');
$im->box(filled=>1, color=>'0000FF', xmin => 6, ymin=>0, xmax => 21, ymax=>15);
$im->box(filled=>1, color=>'00FF00', xmin => 10, ymin=>16, xmax => 25, ymax=>31);

ok($im->write(file=>'testout/t10_32.ico', type=>'ico'),
   "write 32-bit icon");

my $im2 = Imager->new;
ok($im2->read(file=>'testout/t10_32.ico', type=>'ico'),
   "read it back in");

is(Imager::i_img_diff($im->{IMG}, $im2->{IMG}), 0,
   "check they're the same");
is($im->bits, $im2->bits, "check same bits");
