#!perl
use strict;
use warnings;
use Imager;
use Imager::Test qw(test_image_named);
use Test::More;

my %layout_channels =
  (
    rgb =>       [ 0 .. 2 ],
    rgba =>      [ 0 .. 3 ],
    rgb_alpha => [ 0 .. 3 ],
    rgbx =>      [ 0 .. 3 ],
    bgr =>       [ reverse(0 .. 2) ],
    abgr =>      [ reverse(0 .. 3) ],
   );

{
  my @imnames = ( "gray", "gray_a", "basic", "rgba" );

  # basic tests
  for my $chans (1 .. 4) {
    my $im = test_image_named($imnames[$chans-1]);
    is($im->channels, $chans, "$chans: check channel count");

    ok(!defined $im->data(layout => "palette"),
       "$chans: can't get paletted data from direct colour image");

    # test data in base format for this channel count
    my $expect = '';
    for my $y (0 .. $im->getheight()-1) {
      $expect .= $im->getsamples(y => $y);
    }

    {
      my $data = $im->data;
      ok($data, "$chans: got data")
        or diag($im->errstr);

      # is() would produce horrific output comparing this binary data
      ok($data eq $expect, "$chans: data matches expected");
    }
    {
      # make one with extra channels
      my $im2 = Imager->new(xsize => $im->getwidth, ysize => $im->getheight,
                            channels => $chans, extrachannels => 2);
      ok($im2->paste(img => $im), "$chans: past original into extrachannel image");
      ok(!$im2->data, "$chans: fail to get data from extrachannel image without synth or extras");
      my $data = $im2->data(flags => 'synth');
      ok($data, "$chans: got synthesized data");
      ok($data eq $expect, "$chans: synthesized data matches from extra channel image");

      my $extra;
      $data = $im2->data(flags => 'extras', extra => \$extra);
      ok($data, "$chans: got raw data from extra channels image");
      is($extra, 2, "$chans: got expected extra channels");

      # build comparison data
      my $expextra = '';
      for my $y (0 .. $im2->getheight()-1) {
        $expextra .= $im2->getsamples(y => $y, channels => [ 0 .. $im2->totalchannels() -1 ]);
      }
      ok($data eq $expextra, "$chans: extra channel data matches")
        or do {
          diag "data len: ".length $data;
          diag "expect len: ".length $expextra;
        };
    }

    for my $layout (qw(rgb rgba rgb_alpha rgbx bgr abgr)) {
      my $data = $im->data(layout => $layout, flags => "synth");

      my $gchans = $layout_channels{$layout} or die "No channels found for $layout";
      my $lexpect = '';
      # make an rgba version of the image
      my $tim = $im->convert(preset => "rgb")->convert(preset => "addalpha");
      for my $y ( 0 .. $im->getheight()-1 ) {
        $lexpect .= $tim->getsamples(y => $y, channels => $gchans);
      }
      if ($layout eq "rgbx") {
        # rgbx sets channel 3 to zero
        my $i = 3;
        while ($i < length $lexpect) {
          substr($lexpect, $i, 1, "\0");
          $i += 4;
        }
      }
      ok($data eq $lexpect, "$chans: non standard layout $layout works")
        or do {
          diag "Data len: ". length $data;
          diag "Expect len: " . length $lexpect;
          diag "  Data start: " . unpack("H*", substr($data, 0, 30));
          diag "Expect start: " . unpack("H*", substr($lexpect, 0, 30));
        };
    }
  }
}

done_testing();
