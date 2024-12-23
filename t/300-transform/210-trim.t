#!perl -w
use strict;
use Imager;
use Test::More;
use Imager::Test qw(is_image);

{
  my $im8 = Imager->new(channels => 4, xsize => 200, ysize => 400);
  $im8->box(filled => 1, xmin => 11, ymin => 23, xmax => 161, ymax => 257, color => "#FFF") or die;
  my $im16 = $im8->to_rgb16;
  for my $test ( [ $im8, 8 ], [ $im16, 16 ]) {
    my ($im, $bits) = @$test;

    {
      my @rect = $im->trim_rect();
      note "@rect";
      is_deeply(\@rect, [ 11, 23, 38, 142 ], "check alpha trim ($bits)");
    }

    my $imr = $im->rotate(right => 90)
      or die;
    is($imr->bits, $bits, "rotated has expected bits ($bits)");

    {
      my @rect = $imr->trim_rect();
      note "@rect";
      is_deeply(\@rect, [ 142, 11, 23, 38 ], "check alpha trim ($bits)");
    }
  }
}

{
  my $im = Imager->new(channels => 3, xsize => 200, ysize => 300);
  $im->box(filled => 1, color => "#F00");
  $im->box(filled => 1, color => "#00F",
           xmin => 31, ymin => 42, xmax => 116, ymax => 251);
  #$im->write(file => "trim2.png") or die;
  {
    my @rect = $im->trim_rect(colors => [ "#F00" ]);
    note "@rect";
    is_deeply(\@rect, [ 31, 42, 83, 48 ], "check simple color trim");
    my $trimmed = $im->trim(colors => [  "#F00" ]);
    ok($trimmed, "got a simple color trimmed image");
    is_image($trimmed, $im->crop(left => 31, top => 42, right => 117, bottom => 252),
             "check trimmed image is as expected");
  }
  {
    # intrude into the border a little
    $im->box(filled => 1, color => "#000", xmin => 16, xmax => 18, ymin => 8, ymax => 10);
    my @rect = $im->trim_rect(colors => [ "#F00" ]);
    is_deeply(\@rect, [ 16, 8, 83, 48 ],
              "check simple color trim with intrusion");
    my $trimmed = $im->trim(colors => [  "#F00" ]);
    ok($trimmed, "got a simple color trimmed image");
    is_image($trimmed, $im->crop(left => 16, top => 8, right => 117, bottom => 252),
             "check simple with intrusion trimmed image is as expected");
  }
}

{
  my $im = Imager->new(channels => 3, xsize => 20, ysize => 20);
  $im->box(filled => 1, color => "#FFF");
  $im->box(filled => 1, color => "#F00", xmax => 0);
  $im->box(filled => 1, color => "#0F0", ymax => 1);
  $im->box(filled => 1, color => "#00F", xmin => 17);
  $im->box(filled => 1, color => "#FF0", ymin => 16);
  my @rect = $im->trim_rect(colors => [ "#F00", "#0F0", "#00F", "#FF0" ]);
  note "@rect";
  is_deeply(\@rect, [ 1, 2, 3, 4 ], "check multi-color trim");
}

{
  my $im = Imager->new(channels => 4, xsize => 20, ysize => 20);
  $im->box(filled => 1, xmax => 9, color => "#F00");
  $im->box(filled => 1, xmin => 10, color => "#0F0");
  $im->box(filled => 1, color => "#FFF", xmin => 1, ymin => 2, xmax => 16, ymax => 15);
  my $tcl = Imager::TrimColorList->new("#F00", "#0F0");
  my @rect = $im->trim_rect(colors => $tcl);
  note "@rect";
  is_deeply(\@rect, [ 1, 2, 3, 4 ], "check trim via TrimColorList");
}

{
  my $im = Imager->new(channels =>4, xsize => 20, ysize => 20);
  $im->box(filled => 1, color => "#FFF");
  $im->box(filled => 1, color => "#F00", xmax => 0);
  $im->box(filled => 1, color => "#0F0", ymax => 1);
  $im->box(filled => 1, color => "#00F", xmin => 17);
  $im->box(filled => 1, color => "#FF0", ymin => 16);
  my @rect = $im->trim_rect(auto => "center")
    or diag $im->errstr;
  note "@rect";
  is_deeply(\@rect, [ 1, 2, 3, 4 ], "check auto=center trim");
}

{
  my $im = Imager->new(channels => 3, xsize => 20, ysize => 20);
  $im->box(filled => 1, color => "#FFF");
  # off centre cross
  $im->box(filled => 1, xmin => 8, xmax => 11, ymin => 1, ymax => 17, color => "#000");
  $im->box(filled => 1, xmin => 3, xmax => 16, ymin => 10, ymax => 11, color => "#000");
  my @rect = $im->trim_rect(auto => "centre");
  is_deeply(\@rect, [ 3, 1, 3, 2 ], "trim_rect 3-channel cross");
  is_image($im->trim(auto => "center"),
           $im->crop(left => 3, top => 1, right => 17, bottom => 18),
           "trim 3-channel cross");
}

{
  my $im = Imager->new(channels => 4, xsize => 20, ysize => 20);
  # off centre cross, working by transparency instead
  $im->box(filled => 1, xmin => 8, xmax => 11, ymin => 1, ymax => 17, color => "#F00");
  $im->box(filled => 1, xmin => 3, xmax => 16, ymin => 10, ymax => 11, color => "#F00");
  my @rect = $im->trim_rect(auto => "centre");
  is_deeply(\@rect, [ 3, 1, 3, 2 ], "trim_rect 4-channel cross");
  is_image($im->trim(auto => "center"),
           $im->crop(left => 3, top => 1, right => 17, bottom => 18),
           "trim 4-channel cross");
}

done_testing();
