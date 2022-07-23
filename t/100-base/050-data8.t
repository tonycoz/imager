#!perl
use strict;
use warnings;
use Imager;
use Imager::Test qw(test_image_named test_image to_linear_srgb to_linear_srgbf is_arrayf);
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

my %samptypes =
  (
    '8' => '8bit',
    'double' => 'float',
    '16' => '16bit',
    'float' => 'float',
   );

{
  my @imnames = ( "gray", "gray_a", "basic", "rgba" );

  for my $bits (8, "double", 16, 'float') {
    my $gsamp_type = $samptypes{$bits} || die;

    # basic tests
    for my $chans (1 .. 4) {
      my $im = test_image_named($imnames[$chans-1]);
      is($im->channels, $chans, "$chans: check channel count");

      ok(!defined $im->data(layout => "palette", bits => $bits),
         "$chans/$bits: can't get paletted data from direct colour image");

      ok(!defined $im->data(layout => "palette", bits => $bits, flags => 'synth'),
         "$chans/$bits: can't get paletted data from direct colour image, even with synth");

      # test data in base format for this channel count
      my $expect = '';
      if ($bits eq "16") {
        for my $y (0 .. $im->getheight()-1) {
          $expect .= pack("S*", $im->getsamples(y => $y, type => $gsamp_type));
        }
      }
      elsif ($bits eq "float") {
        for my $y (0 .. $im->getheight()-1) {
          $expect .= pack("f*", $im->getsamples(y => $y, type => 'float'));
        }
      }
      else {
        for my $y (0 .. $im->getheight()-1) {
          $expect .= $im->getsamples(y => $y, type => $gsamp_type);
        }
      }

      if ($bits eq "8") {
        my $data = $im->data;
        ok($data, "$chans/$bits: got data")
          or diag($im->errstr);

        # is() would produce horrific output comparing this binary data
        ok($data eq $expect, "$chans/$bits: data matches expected");
      }
      {
        # make one with extra channels
        my $im2 = Imager->new(xsize => $im->getwidth, ysize => $im->getheight,
                              channels => $chans, extrachannels => 2);
        ok($im2->paste(img => $im), "$chans/$bits: paste original into extrachannel image");
        ok(!$im2->data, "$chans/$bits: fail to get data from extrachannel image without synth or extras");
        my $data = $im2->data(flags => 'synth', bits => $bits);
        ok($data, "$chans/$bits: got synthesized data");
        ok($data eq $expect, "$chans/$bits: synthesized data matches from extra channel image");

        if ($bits eq "8") {
          my $extra;
          $data = $im2->data(flags => 'extras', extra => \$extra, bits => $bits);
          ok($data, "$chans/$bits: got raw data from extra channels image");
          is($extra, 2, "$chans/$bits: got expected extra channels");

          # build comparison data
          my $expextra = '';
          for my $y (0 .. $im2->getheight()-1) {
            $expextra .= $im2->getsamples(y => $y, channels => [ 0 .. $im2->totalchannels() -1 ],
                                          type => $gsamp_type);
          }
          ok($data eq $expextra, "$chans/$bits: extra channel data matches")
            or do {
              diag "data len: ".length $data;
              diag "expect len: ".length $expextra;
            };
        }
      }

      for my $layout (qw(rgb rgba rgb_alpha rgbx bgr abgr)) {
        my $data = $im->data(layout => $layout, flags => "synth", bits => $bits);

        my $gchans = $layout_channels{$layout} or die "No channels found for $layout";
        my $lexpect = '';
        # make an rgba version of the image
        my $tim = $im->convert(preset => "rgb")->convert(preset => "addalpha");
        if ($bits eq "16") {
          for my $y ( 0 .. $im->getheight()-1 ) {
            $lexpect .= pack "S*", $tim->getsamples(y => $y, channels => $gchans, type => $gsamp_type);
          }
        }
        elsif ($bits eq "float") {
          for my $y ( 0 .. $im->getheight()-1 ) {
            $lexpect .= pack "f*", $tim->getsamples(y => $y, channels => $gchans, type => "float");
          }
        }
        else {
          for my $y ( 0 .. $im->getheight()-1 ) {
            $lexpect .= $tim->getsamples(y => $y, channels => $gchans, type => $gsamp_type);
          }
        }
        if ($layout eq "rgbx") {
          # rgbx sets channel 3 to zero
          my $replace =
            $bits eq "8" ? "\0" :
            $bits eq "16" ? "\0\0" :
            $bits eq "float" ? pack("f", 0.0) : pack("d", 0.0);
          my $size = length $replace;
          my $i = 3;
          while ($i * $size < length $lexpect) {
            substr($lexpect, $i * $size, $size, $replace);
            $i += 4;
          }
        }
        ok($data eq $lexpect, "$chans/$bits: non standard layout $layout works")
          or do {
            diag "Data len: ". length $data;
            diag "Expect len: " . length $lexpect;
            diag "  Data start: " . unpack("H*", substr($data, 0, 30));
            diag "Expect start: " . unpack("H*", substr($lexpect, 0, 30));
          };
      }
    }
  }
}

{
  # linear
  my $im = test_image; {
    my $lindata = $im->data(flags => "linear,synth", bits => 16);
    my $rawdata = $im->data();
    my @rawdata = unpack "C*", $rawdata;

    my @expect = map { to_linear_srgb($_) } @rawdata;
    my @lindata = unpack "S*", $lindata;
    is_deeply(\@lindata, \@expect, "test linear data vs translated");
  }
  {
    my $flindata = $im->data(flags => "linear,synth", bits => "double");
    my @flindata = unpack "d*", $flindata;
    my $frawdata = $im->data(flags => "synth", bits => "double");
    my @frawdata = unpack "d*", $frawdata;
    my @fexpect = map { to_linear_srgbf($_) } @frawdata;
    is_arrayf(\@flindata, \@fexpect, "test linear double vs translated");
  }
}

done_testing();
