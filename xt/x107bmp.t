#!perl -w
# Extra BMP tests not shipped
use strict;
use Test::More;
use Imager::Test qw(is_image);
use Imager;

# test images from 
my @tests =
  (
   [ "g01bg.bmp", "1-bit blue/green", 0 ],
   [ "g01bw.bmp", "1-bit black and white", 0 ],
   [ "g01p1.bmp", "1-bit single colour", 0 ],
   [ "g01wb.bmp", "1-bit white and black", 0 ],
   [ "g04.bmp", "4-bit", 0 ],
   [ "g04p4.bmp", "4-bit gray", 0 ],
   [ "g04rle.bmp", "4-bit rle", "currently broken" ],
   [ "g08.bmp", "8-bit", 0 ],
   [ "g08offs.bmp", "8-bit with image data offset", 0 ],
   [ "g08os2.bmp", "8-bit OS/2", "OS/2 BMP not implemented" ],
   [ "g08p256.bmp", "8-bit, no important", 0 ],
   [ "g08p64.bmp", "8-bit, 64 greyscale entries", 0 ],
   [ "g08pi256.bmp", "8-bit 256 important", 0 ],
   [ "g08pi64.bmp", "8-bit 64 important", 0 ],
   [ "g08res11.bmp", "8-bit, 100x100 dpi", 0 ],
   [ "g08res21.bmp", "8-bit, 200x100 dpi", 0 ],
   [ "g08res22.bmp", "8-bit, 200x200 dpi", 0 ],
   [ "g08rle.bmp", "8-bit rle", 0 ],
   [ "g08s0.bmp", "8-bit, bits size not given", 0 ],
   [ "g08w124.bmp", "8-bit 124x61", 0 ],
   [ "g08w125.bmp", "8-bit 125x62", 0 ],
   [ "g08w126.bmp", "8-bit 126x63", 0 ],
   [ "g16bf555.bmp", "16-bit bitfield 555", 0 ],
   [ "g16bf565.bmp", "16-bit bitfield 565", 0 ],
   [ "g16def555.bmp", "16-bit default 555", 0 ],
   [ "g24.bmp", "24-bit", 0 ],
   [ "g32bf.bmp", "32-bit bitfields", 0 ],
   [ "g32def.bmp", "32-bit defaults", 0 ],
   [ "test32bfv4.bmp", "32-bit bitfields, v4", "v4 BMP not implemented" ],
   [ "test32v5.bmp", "32-bit, v5", "v5 BMP not implemented" ],
   [ "test4os2v2.bmp", "4-bit OS/2", "OS/2 BMP not implemented" ],
   [ "trans.bmp", "transparency", "alpha BMPs not implemented" ],
   [ "width.bmp", "odd-width rle", "currently broken" ],
  );

Imager->open_log(log => "testout/x107bmp.log");

plan tests => 3 * @tests;

for my $test (@tests) {
  my ($in, $note, $todo) = @$test;

  my $im = Imager->new(file => "xtestimg/bmp/$in");
  local $TODO = $todo;
  ok($im, "load $in ($note)")
    or diag "$in: ".Imager->errstr;
  (my $alt = $in) =~ s/\.bmp$/.sgi/;

  my $ref = Imager->new(file => "xtestimg/bmp/$alt");
  {
    local $TODO; # should always pass
    ok($ref, "load reference image for $in")
      or diag "$alt: ".Imager->errstr;
    if ($ref->getchannels == 1) {
      $ref = $ref->convert(preset => "rgb");
    }
  }
  is_image($im, $ref, "compare $note");
}

Imager->close_log();

