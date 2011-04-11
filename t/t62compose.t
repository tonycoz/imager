#!perl -w
use strict;
use Imager qw(:handy);
use Test::More tests => 50;
use Imager::Test qw(is_image);

-d "testout" or mkdir "testout";

Imager::init_log("testout/t62compose.log", 1);

{
  my $blue = NC(0, 0, 255);
  my $red =  NC(255, 0, 0);
  my $green2 = NC(0, 255, 0, 128);
  my $green2_on_blue = NC(0, 128, 127);
  my $red3_on_blue = NC(85, 0, 170);
  my $green6_on_blue = NC(0, 42, 213);

  my $targ = Imager->new(xsize => 100, ysize => 100);
  $targ->box(color => $blue, filled => 1);
  is($targ->type, "direct", "check target image type");
  is($targ->bits, 8, "check target bits");

  my $src = Imager->new(xsize => 40, ysize => 40, channels => 4);
  $src->box(filled => 1, color => $red, xmax => 19, ymax => 19);
  $src->box(filled => 1, xmin => 20, color => $green2);

  my $mask_ones = Imager->new(channels => 1, xsize => 40, ysize => 40);
  $mask_ones->box(filled => 1, color => NC(255, 255, 255));
			    

  # mask or full mask, should be the same
  for my $mask_info ([ "no mask" ], [ "full mask", mask => $mask_ones ]) {
    my ($mask_type, @mask_extras) = @$mask_info;
    print "# $mask_type\n";
    {
      my $cmp = $targ->copy;
      $cmp->box(filled => 1, color => $red,
		xmin=> 5, ymin => 10, xmax => 24, ymax => 29);
      $cmp->box(filled => 1, color => $green2_on_blue,
		xmin => 25, ymin => 10, xmax => 44, ymax => 49);
      {
	my $work = $targ->copy;
	ok($work->compose(src => $src, tx => 5, ty => 10, @mask_extras),
	   "$mask_type - simple compose");
	is_image($work, $cmp, "check match");
      }
      { # >1 opacity
	my $work = $targ->copy;
	ok($work->compose(src => $src, tx => 5, ty => 10, opacity => 2.0, @mask_extras),
	   "$mask_type - compose with opacity > 1.0 acts like opacity=1.0");
	is_image($work, $cmp, "check match");
      }
      { # 0 opacity is a failure
	my $work = $targ->copy;
	ok(!$work->compose(src => $src, tx => 5, ty => 10, opacity => 0.0, @mask_extras),
	   "$mask_type - compose with opacity = 0 is an error");
	is($work->errstr, "opacity must be positive", "check message");
      }
    }
    { # compose at 1/3
      my $work = $targ->copy;
      ok($work->compose(src => $src, tx => 7, ty => 33, opacity => 1/3, @mask_extras),
	 "$mask_type - simple compose at 1/3");
      my $cmp = $targ->copy;
      $cmp->box(filled => 1, color => $red3_on_blue,
		xmin => 7, ymin => 33, xmax => 26, ymax => 52);
      $cmp->box(filled => 1, color => $green6_on_blue,
		xmin => 27, ymin => 33, xmax => 46, ymax => 72);
      is_image($work, $cmp, "check match");
    }
    { # targ off top left
      my $work = $targ->copy;
      ok($work->compose(src => $src, tx => -5, ty => -3, @mask_extras),
	 "$mask_type - compose off top left");
      my $cmp = $targ->copy;
      $cmp->box(filled => 1, color => $red,
		xmin=> 0, ymin => 0, xmax => 14, ymax => 16);
      $cmp->box(filled => 1, color => $green2_on_blue,
		xmin => 15, ymin => 0, xmax => 34, ymax => 36);
      is_image($work, $cmp, "check match");
    }
    { # targ off bottom right
      my $work = $targ->copy;
      ok($work->compose(src => $src, tx => 65, ty => 67, @mask_extras),
	 "$mask_type - targ off bottom right");
      my $cmp = $targ->copy;
      $cmp->box(filled => 1, color => $red,
		xmin=> 65, ymin => 67, xmax => 84, ymax => 86);
      $cmp->box(filled => 1, color => $green2_on_blue,
		xmin => 85, ymin => 67, xmax => 99, ymax => 99);
      is_image($work, $cmp, "check match");
    }
    { # src off top left
      my $work = $targ->copy;
      ok($work->compose(src => $src, tx => 10, ty => 20,
			src_left => -5, src_top => -15, @mask_extras),
	 "$mask_type - source off top left");
      my $cmp = $targ->copy;
      $cmp->box(filled => 1, color => $red,
		xmin=> 15, ymin => 35, xmax => 34, ymax => 54);
      $cmp->box(filled => 1, color => $green2_on_blue,
	      xmin => 35, ymin => 35, xmax => 54, ymax => 74);
      is_image($work, $cmp, "check match");
    }
    {
      # src off bottom right
      my $work = $targ->copy;
      ok($work->compose(src => $src, tx => 10, ty => 20,
			src_left => 10, src_top => 15,
			width => 40, height => 40, @mask_extras),
	 "$mask_type - source off bottom right");
      my $cmp = $targ->copy;
      $cmp->box(filled => 1, color => $red,
		xmin=> 10, ymin => 20, xmax => 19, ymax => 24);
      $cmp->box(filled => 1, color => $green2_on_blue,
		xmin => 20, ymin => 20, xmax => 39, ymax => 44);
      is_image($work, $cmp, "check match");
    }
    {
      # simply out of bounds
      my $work = $targ->copy;
      ok(!$work->compose(src => $src, tx => 100, @mask_extras),
	 "$mask_type - off the right of the target");
      is_image($work, $targ, "no changes");
      ok(!$work->compose(src => $src, ty => 100, @mask_extras),
	 "$mask_type - off the bottom of the target");
      is_image($work, $targ, "no changes");
      ok(!$work->compose(src => $src, tx => -40, @mask_extras),
	 "$mask_type - off the left of the target");
      is_image($work, $targ, "no changes");
      ok(!$work->compose(src => $src, ty => -40, @mask_extras),
	 "$mask_type - off the top of the target");
      is_image($work, $targ, "no changes");
    }
  }
}
