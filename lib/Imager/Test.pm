package Imager::Test;
use strict;
use Test::Builder;
require Exporter;
use vars qw(@ISA @EXPORT_OK);
@ISA = qw(Exporter);
@EXPORT_OK = qw(diff_text_with_nul test_image_raw test_image_16 is_color3 is_color1 is_image);

sub diff_text_with_nul {
  my ($desc, $text1, $text2, @params) = @_;

  my $builder = Test::Builder->new;

  print "# $desc\n";
  my $imbase = Imager->new(xsize => 100, ysize => 100);
  my $imcopy = Imager->new(xsize => 100, ysize => 100);

  $builder->ok($imbase->string(x => 5, 'y' => 50, size => 20,
			       string => $text1,
			       @params), "$desc - draw text1");
  $builder->ok($imcopy->string(x => 5, 'y' => 50, size => 20,
			       string => $text2,
			       @params), "$desc - draw text2");
  $builder->isnt_num(Imager::i_img_diff($imbase->{IMG}, $imcopy->{IMG}), 0,
		     "$desc - check result different");
}

sub is_color3($$$$$) {
  my ($color, $red, $green, $blue, $comment) = @_;

  my $builder = Test::Builder->new;

  unless (defined $color) {
    $builder->ok(0, $comment);
    $builder->diag("color is undef");
    return;
  }
  unless ($color->can('rgba')) {
    $builder->ok(0, $comment);
    $builder->diag("color is not a color object");
    return;
  }

  my ($cr, $cg, $cb) = $color->rgba;
  unless ($builder->ok($cr == $red && $cg == $green && $cb == $blue, $comment)) {
    $builder->diag(<<END_DIAG);
Color mismatch:
  Red: $red vs $cr
Green: $green vs $cg
 Blue: $blue vs $cb
END_DIAG
    return;
  }

  return 1;
}

sub is_color1($$$) {
  my ($color, $grey, $comment) = @_;

  my $builder = Test::Builder->new;

  unless (defined $color) {
    $builder->ok(0, $comment);
    $builder->diag("color is undef");
    return;
  }
  unless ($color->can('rgba')) {
    $builder->ok(0, $comment);
    $builder->diag("color is not a color object");
    return;
  }

  my ($cgrey) = $color->rgba;
  unless ($builder->ok($cgrey == $grey, $comment)) {
    $builder->diag(<<END_DIAG);
Color mismatch:
  Grey: $grey vs $cgrey
END_DIAG
    return;
  }

  return 1;
}

sub test_image_raw {
  my $green=Imager::i_color_new(0,255,0,255);
  my $blue=Imager::i_color_new(0,0,255,255);
  my $red=Imager::i_color_new(255,0,0,255);
  
  my $img=Imager::ImgRaw::new(150,150,3);
  
  Imager::i_box_filled($img,70,25,130,125,$green);
  Imager::i_box_filled($img,20,25,80,125,$blue);
  Imager::i_arc($img,75,75,30,0,361,$red);
  Imager::i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

  $img;
}

sub test_image_16 {
  my $green = Imager::Color->new(0, 255, 0, 255);
  my $blue  = Imager::Color->new(0, 0, 255, 255);
  my $red   = Imager::Color->new(255, 0, 0, 255);
  my $img = Imager->new(xsize => 150, ysize => 150, bits => 16);
  $img->box(filled => 1, color => $green, box => [ 70, 25, 130, 125 ]);
  $img->box(filled => 1, color => $blue,  box => [ 20, 25, 80, 125 ]);
  $img->arc(x => 75, y => 75, r => 30, color => $red);
  $img->filter(type => 'conv', coef => [ 0.1, 0.2, 0.4, 0.2, 0.1 ]);

  $img;
}

sub is_image($$$) {
  my ($left, $right, $comment) = @_;

  my $builder = Test::Builder->new;

  unless (defined $left) {
    $builder->ok(0, $comment);
    $builder->diag("left is undef");
    return;
  } 
  unless (defined $right) {
    $builder->ok(0, $comment);
    $builder->diag("right is undef");
    return;
  }
  unless ($left->{IMG}) {
    $builder->ok(0, $comment);
    $builder->diag("left image has no low level object");
    return;
  }
  unless ($right->{IMG}) {
    $builder->ok(0, $comment);
    $builder->diag("right image has no low level object");
    return;
  }
  unless ($left->getwidth == $right->getwidth) {
    $builder->ok(0, $comment);
    $builder->diag("left width " . $left->getwidth . " vs right width " 
                   . $right->getwidth);
    return;
  }
  unless ($left->getheight == $right->getheight) {
    $builder->ok(0, $comment);
    $builder->diag("left height " . $left->getheight . " vs right height " 
                   . $right->getheight);
    return;
  }
  unless ($left->getchannels == $right->getchannels) {
    $builder->ok(0, $comment);
    $builder->diag("left channels " . $left->getchannels . " vs right channels " 
                   . $right->getchannels);
    return;
  }
  my $diff = Imager::i_img_diff($left->{IMG}, $right->{IMG});
  unless ($diff == 0) {
    $builder->ok(0, $comment);
    $builder->diag("image data different - $diff");
    return;
  }
  
  return $builder->ok(1, $comment);
}

1;

__END__

=head1 NAME

Imager::Test - common functions used in testing Imager

=head1 SYNOPSIS

  use Imager::Test 'diff_text_with_nul';
  diff_text_with_nul($test_name, $text1, $text2, @string_options);

=head1 DESCRIPTION

This is a repository of functions used in testing Imager.

Some functions will only be useful in testing Imager itself, while
others should be useful in testing modules that use Imager.

No functions are exported by default.

=head1 FUNCTIONS

=over

=item is_color3($color, $red, $blue, $green, $comment)

Tests is $color matches the given ($red, $blue, $green)

=item is_image($im1, $im2, $comment)

Tests if the 2 images have the same content.  Both images must be
defined, have the same width, height, channels and the same color in
each pixel.  The color comparison is done at 8-bits per pixel.  The
color representation such as direct vs paletted, bits per sample are
not checked.

=item test_image_raw()

Returns a 150x150x3 Imager::ImgRaw test image.

=item test_image_16()

Returns a 150x150x3 16-bit/sample OO test image.

=item diff_text_with_nul($test_name, $text1, $text2, @options)

Creates 2 test images and writes $text1 to the first image and $text2
to the second image with the string() method.  Each call adds 3 ok/not
ok to the output of the test script.

Extra options that should be supplied include the font and either a
color or channel parameter.

This was explicitly created for regression tests on #21770.

=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=cut
