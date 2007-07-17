#!perl -w
use strict;
use Imager;
use Test::More tests => 24;
use Imager::Test qw(test_image test_image_16 is_image);

Imager::init_log('testout/20write.log', 2);

{
  my $im = test_image();
  ok($im->write(file => 'testout/20verb.rgb'), "write 8-bit verbatim")
    or print "# ", $im->errstr, "\n";
  my $im2 = Imager->new;
  ok($im2->read(file => 'testout/20verb.rgb'), "read it back")
    or print "# ", $im2->errstr, "\n";
  is_image($im, $im2, "compare");
  is($im2->tags(name => 'sgi_rle'), 0, "check not rle");
  is($im2->tags(name => 'sgi_bpc'), 1, "check bpc");
  is($im2->tags(name => 'i_comment'), '', "no namestr");
  
  ok($im->write(file => 'testout/20rle.rgb', 
		sgi_rle => 1, 
		i_comment => "test"), "write 8-bit rle")
    or print "# ", $im->errstr, "\n";
  my $im3 = Imager->new;
  ok($im3->read(file => 'testout/20rle.rgb'), "read it back")
    or print "# ", $im3->errstr, "\n";
  is_image($im, $im3, "compare");
  is($im3->tags(name => 'sgi_rle'), 1, "check not rle");
  is($im3->tags(name => 'sgi_bpc'), 1, "check bpc");
  is($im3->tags(name => 'i_comment'), 'test', "check i_comment set");
}

{
  my $im = test_image_16();
  ok($im->write(file => 'testout/20verb16.rgb'), "write 16-bit verbatim")
    or print "# ", $im->errstr, "\n";
  my $im2 = Imager->new;
  ok($im2->read(file => 'testout/20verb16.rgb'), "read it back")
    or print "# ", $im2->errstr, "\n";
  is_image($im, $im2, "compare");
  is($im2->tags(name => 'sgi_rle'), 0, "check not rle");
  is($im2->tags(name => 'sgi_bpc'), 2, "check bpc");
  is($im2->tags(name => 'i_comment'), '', "no namestr");
  
  ok($im->write(file => 'testout/20rle16.rgb', 
		sgi_rle => 1, 
		i_comment => "test"), "write 16-bit rle")
    or print "# ", $im->errstr, "\n";
  my $im3 = Imager->new;
  ok($im3->read(file => 'testout/20rle16.rgb'), "read it back")
    or print "# ", $im3->errstr, "\n";
  is_image($im, $im3, "compare");
  is($im3->tags(name => 'sgi_rle'), 1, "check not rle");
  is($im3->tags(name => 'sgi_bpc'), 2, "check bpc");
  is($im3->tags(name => 'i_comment'), 'test', "check i_comment set");
}

