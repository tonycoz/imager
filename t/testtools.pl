# this doesn't need a new namespace - I hope
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

sub okx {
  my ($ok, $comment) = @_;

  return okn($TESTNUM++, $ok, $comment);
}

sub okn {
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

sub matchn($$$$) {
  my ($num, $str, $re, $comment) = @_;

  my $match = $str =~ $re;
  okn($num, $match, $comment);
  unless ($match) {
    $str =~ s/\\/\\\\/g;
    $str =~ s/[^\x20-\x7E]/"\\x".sprintf("%02X", ord($1))/ge;
    print "# The string '$str'\n";
    print "# did not match '$re'\n";
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

