# this doesn't need a new namespace - I hope
use strict;
use Imager qw(:all);
use vars qw($TESTNUM);
use Carp 'confess';

$TESTNUM = 1;

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

sub test_oo_img {
  my $raw = test_img();
  my $img = Imager->new;
  $img->{IMG} = $raw;

  $img;
}

sub skipn {
  my ($testnum, $count, $why) = @_;
  
  $why = '' unless defined $why;

  print "ok $_ # skip $why\n" for $testnum ... $testnum+$count-1;
}

sub skipx {
  my ($count, $why) = @_;

  skipn($TESTNUM, $count, $why);
  $TESTNUM += $count;
}

sub okx ($$) {
  my ($ok, $comment) = @_;

  return okn($TESTNUM++, $ok, $comment);
}

sub okn ($$$) {
  my ($num, $ok, $comment) = @_;

  defined $num or confess "No \$num supplied";
  defined $comment or confess "No \$comment supplied";
  if ($ok) {
    print "ok $num # $comment\n";
  }
  else {
    print "not ok $num # $comment\n";
  }

  return $ok;
}

sub requireokx {
  my ($file, $comment) = @_;

  eval {
    require $file;
  };
  if ($@) {
    my $msg = $@;
    $msg =~ s/\n+$//;
    $msg =~ s/\n/\n# /g;
    okx(0, $comment);
    print "# $msg\n";
  }
  else {
    okx(1, $comment);
  }
}

sub useokx {
  my ($module, $comment, @imports) = @_;
  
  my $pack = caller;
  eval <<EOS;
package $pack;
require $module;
$module->import(\@imports);
EOS
  unless (okx(!$@, $comment)) {
    my $msg = $@;
    $msg =~ s/\n+$//;
    $msg =~ s/\n/\n# /g;
    print "# $msg\n";
    return 0;
  }
  else {
    return 1;
  }
}

sub matchn($$$$) {
  my ($num, $str, $re, $comment) = @_;

  my $match = defined($str) && $str =~ $re;
  okn($num, $match, $comment);
  unless ($match) {
    print "# The value: ",_sv_str($str),"\n";
    print "# did not match: qr/$re/\n";
  }
  return $match;
}

sub matchx($$$) {
  my ($str, $re, $comment) = @_;

  matchn($TESTNUM++, $str, $re, $comment);
}

sub isn ($$$$) {
  my ($num, $left, $right, $comment) = @_;

  my $match;
  if (!defined $left && defined $right
     || defined $left && !defined $right) {
    $match = 0;
  }
  elsif (!defined $left && !defined $right) {
    $match = 1;
  }
  # the right of the || produces a string of \0 if $left is a PV
  # which is true
  elsif (!length $left  || ($left & ~$left) ||
	 !length $right || ($right & ~$right)) {
    $match = $left eq $right;
  }
  else {
    $match = $left == $right;
  }
  okn($num, $match, $comment);
  unless ($match) {
    print "# the following two values were not equal:\n";
    print "# value: ",_sv_str($left),"\n";
    print "# other: ",_sv_str($right),"\n";
  }

  $match;
}

sub isx ($$$) {
  my ($left, $right, $comment) = @_;

  isn($TESTNUM++, $left, $right, $comment);
}

sub _sv_str {
  my ($value) = @_;

  if (defined $value) {
    if (!length $value || ($value & ~$value)) {
      $value =~ s/\\/\\\\/g;
      $value =~ s/\r/\\r/g;
      $value =~ s/\n/\\n/g;
      $value =~ s/\t/\\t/g;
      $value =~ s/\"/\\"/g;
      $value =~ s/([^ -\x7E])/"\\x".sprintf("%02x", ord($1))/ge;

      return qq!"$value"!;
    }
    else {
      return $value; # a number
    }
  }
  else {
    return "undef";
  }
}


1;

sub test_colorf_gpix {
  my ($im, $x, $y, $expected, $epsilon) = @_;
  my $c = Imager::i_gpixf($im, $x, $y);
  ok($c, "got gpix ($x, $y)");
  unless (ok(colorf_cmp($c, $expected, $epsilon) == 0,
	     "got right color ($x, $y)")) {
    print "# got: (", join(",", ($c->rgba)[0,1,2]), ")\n";
    print "# expected: (", join(",", ($expected->rgba)[0,1,2]), ")\n";
  }
}

sub test_color_gpix {
  my ($im, $x, $y, $expected) = @_;
  my $c = Imager::i_get_pixel($im, $x, $y);
  ok($c, "got gpix ($x, $y)");
  unless (ok(color_cmp($c, $expected) == 0,
     "got right color ($x, $y)")) {
    print "# got: (", join(",", ($c->rgba)[0,1,2]), ")\n";
    print "# expected: (", join(",", ($expected->rgba)[0,1,2]), ")\n";
  }
}

sub test_colorf_glin {
  my ($im, $x, $y, @pels) = @_;

  my @got = Imager::i_glinf($im, $x, $x+@pels, $y);
  is(@got, @pels, "check number of pixels ($x, $y)");
  ok(!grep(colorf_cmp($pels[$_], $got[$_], 0.005), 0..$#got),
     "check colors ($x, $y)");
}

sub colorf_cmp {
  my ($c1, $c2, $epsilon) = @_;

  defined $epsilon or $epsilon = 0;

  my @s1 = $c1->rgba;
  my @s2 = $c2->rgba;

  # print "# (",join(",", @s1[0..2]),") <=> (",join(",", @s2[0..2]),")\n";
  return abs($s1[0]-$s2[0]) >= $epsilon && $s1[0] <=> $s2[0] 
    || abs($s1[1]-$s2[1]) >= $epsilon && $s1[1] <=> $s2[1]
      || abs($s1[2]-$s2[2]) >= $epsilon && $s1[2] <=> $s2[2];
}

sub color_cmp {
  my ($c1, $c2) = @_;

  my @s1 = $c1->rgba;
  my @s2 = $c2->rgba;

  return $s1[0] <=> $s2[0] 
    || $s1[1] <=> $s2[1]
      || $s1[2] <=> $s2[2];
}

# these test the action of the channel mask on the image supplied
# which should be an OO image.
sub mask_tests {
  my ($im, $epsilon) = @_;

  defined $epsilon or $epsilon = 0;

  # we want to check all four of ppix() and plin(), ppix() and plinf()
  # basic test procedure:
  #   first using default/all 1s mask, set to white
  #   make sure we got white
  #   set mask to skip a channel, set to grey
  #   make sure only the right channels set

  print "# channel mask tests\n";
  # 8-bit color tests
  my $white = NC(255, 255, 255);
  my $grey = NC(128, 128, 128);
  my $white_grey = NC(128, 255, 128);

  print "# with ppix\n";
  ok($im->setmask(mask=>~0), "set to default mask");
  ok($im->setpixel(x=>0, 'y'=>0, color=>$white), "set to white all channels");
  test_color_gpix($im->{IMG}, 0, 0, $white);
  ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  ok($im->setpixel(x=>0, 'y'=>0, color=>$grey), "set to grey, no channel 2");
  test_color_gpix($im->{IMG}, 0, 0, $white_grey);

  print "# with plin\n";
  ok($im->setmask(mask=>~0), "set to default mask");
  ok($im->setscanline(x=>0, 'y'=>1, pixels => [$white]), 
     "set to white all channels");
  test_color_gpix($im->{IMG}, 0, 1, $white);
  ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  ok($im->setscanline(x=>0, 'y'=>1, pixels=>[$grey]), 
     "set to grey, no channel 2");
  test_color_gpix($im->{IMG}, 0, 1, $white_grey);

  # float color tests
  my $whitef = NCF(1.0, 1.0, 1.0);
  my $greyf = NCF(0.5, 0.5, 0.5);
  my $white_greyf = NCF(0.5, 1.0, 0.5);

  print "# with ppixf\n";
  ok($im->setmask(mask=>~0), "set to default mask");
  ok($im->setpixel(x=>0, 'y'=>2, color=>$whitef), "set to white all channels");
  test_colorf_gpix($im->{IMG}, 0, 2, $whitef, $epsilon);
  ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  ok($im->setpixel(x=>0, 'y'=>2, color=>$greyf), "set to grey, no channel 2");
  test_colorf_gpix($im->{IMG}, 0, 2, $white_greyf, $epsilon);

  print "# with plinf\n";
  ok($im->setmask(mask=>~0), "set to default mask");
  ok($im->setscanline(x=>0, 'y'=>3, pixels => [$whitef]), 
     "set to white all channels");
  test_colorf_gpix($im->{IMG}, 0, 3, $whitef, $epsilon);
  ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  ok($im->setscanline(x=>0, 'y'=>3, pixels=>[$greyf]), 
     "set to grey, no channel 2");
  test_colorf_gpix($im->{IMG}, 0, 3, $white_greyf, $epsilon);

}

sub NCF {
  return Imager::Color::Float->new(@_);
}
