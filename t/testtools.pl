# this doesn't need a new namespace - I hope
use Imager qw(:all);

sub test_img {
  my $green=i_color_new(0,255,0,255);
  my $blue=i_color_new(0,0,255,255);
  my $red=i_color_new(255,0,0,255);
  
  my $img=Imager::ImgRaw::new(150,150,3);
  
  i_box_filled($img,70,25,130,125,$green);
  i_box_filled($img,20,25,80,125,$blue);
  i_arc($img,75,75,30,0,361,$red);
  i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

  $img;
}

sub skipn {
  my ($testnum, $count, $why) = @_;
  
  $why = '' unless defined $why;

  print "ok $_ # skip $why\n" for $testnum ... $testnum+$count-1;
}

1;

