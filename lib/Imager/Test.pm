package Imager::Test;
use strict;
use Test::Builder;
require Exporter;
use vars qw(@ISA @EXPORT_OK);
@ISA = qw(Exporter);
@EXPORT_OK = qw(diff_text_with_nul);

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

=item diff_text_with_nul($test_name, $text1, $text2, @optios)

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
