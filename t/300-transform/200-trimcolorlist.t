#!perl -w
use strict;
use Imager qw(:handy);
use Scalar::Util qw(reftype);
use Test::More;

{
  my $t = Imager::TrimColorList->new;
  ok($t, "make empty list");
  is($t->count, 0, "it's empty");
  is($t->get(0), undef, "nothing at [0]");
  is_deeply([ $t->all ], [], "empty all()");
}

{
  my $cffeecc = NC(0xff, 0xee, 0xcc);
  my $cf0e0c0 = NC(0xF0, 0xE0, 0xC0);
  my $f_572 = NCF(0.5, 0.7, 0.2);
  my $f_683 = NCF(0.6, 0.8, 0.3);
  my @tests =
    ( # name, input, expected
      [ "simple color", "#ffeecc", [ $cffeecc, $cffeecc ] ],
      [ "simple color object", $cffeecc, [ $cffeecc, $cffeecc ] ],
      [ "simple fcolor", $f_572, [ $f_572, $f_572 ] ],
      [ "single color in array", [ "#ffeecc" ], [ $cffeecc, $cffeecc ] ],
      [ "single color object in array", [ $cffeecc ], [ $cffeecc, $cffeecc ] ],
      [ "single fcolor", [ $f_572 ], [ $f_572, $f_572 ] ],
      [ "simple range", [ "#f0e0c0", "#ffeecc" ], [ $cf0e0c0, $cffeecc ] ],
      [ "simple fcolor range", [ $f_572, $f_683 ], [ $f_572, $f_683 ] ],
      [ "color with error amount", [ "#808080", 0.01 ], [ NC(0x7E, 0x7E, 0x7E), NC(0x82, 0x82, 0x82) ] ],
      [
	"fcolor with error amount",
	[ NCF(0.4, 0.5, 0.6), 0.01 ],
	[ NCF(0.39, 0.49, 0.59), NCF(0.41, 0.51, 0.61) ],
       ],
     );
  for my $test (@tests) {
    my ($name, $input, $expect) = @$test;
    my $t = Imager::TrimColorList->new;
    ok($t->add($input), "$name: add")
      or diag "$name: " . Imager->errstr;
    is($t->count, 1, "$name: and it shows in count");
    my $e = $t->get(0);
    is_entry($e, $expect, $name);
  }
}

done_testing();

# is() doesn't really work for the objects we find in the trim color list
sub is_entry {
  my ($testme, $expected, $name) = @_;

  # entries are always an arrayref of 2 elements
  if (!ref($testme) || reftype($testme) ne "ARRAY" || @$testme != 2) {
    fail("$name: not a 2 element arrayref");
  }
  my $testme_text = _format_entry($testme);
  my $expected_text = _format_entry($expected);
  note "testme: ".join(", ", @$testme_text);
  note "expect: ".join(", ", @$expected_text);
  is_deeply($testme_text, $expected_text, $name);
}

sub _format_entry {
  my ($e) = @_;

  return [
    _format_color($e->[0]),
    _format_color($e->[1])
    ];
}

sub _format_color {
  my ($c) = @_;

  # trim color doesn't care about alpha
  if ($c->isa("Imager::Color")) {
    return sprintf("NC(%d,%d,%d)", $c->red, $c->green, $c->blue);
  }
  elsif ($c->isa("Imager::Color::Float")) {
    return sprintf("NCF(%.4g,%.4g,%.4g)", $c->red, $c->green, $c->blue);
  }
  die "Unexpected color $c";
}
