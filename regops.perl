#!perl -w
use strict;
use Data::Dumper;
my $in = shift or die "No input name";
my $out = shift or die "No output name";
open(IN, $in) or die "Cannot open input $in: $!";
open(OUT, "> $out") or die "Cannot create $out: $!";
binmode OUT;
print OUT <<'EOS';
# AUTOMATICALLY GENERATED BY regops.perl
package Imager::Regops;
use 5.006;
use strict;
require Exporter;
our @ISA = qw(Exporter);
our @EXPORT_OK = qw(%Attr $MaxOperands $PackCode);
our $VERSION = "1.000";

EOS
my @ops;
my %attr;
my $opcode = 0;
my $max_opr = 0;
my $reg_pack;
while (<IN>) {
  if (/^\s*rbc_(\w+)/) {
    my $op = $1;
    push(@ops, uc "RBC_$op");
    # each line has a comment with the registers used - find the maximum
    # I could probably do this as one line, but let's not
    my @parms = /\b([rp][a-z])\b/g;
    $max_opr = @parms if @parms > $max_opr;
    my $types = join("", map {substr($_,0,1)} @parms);
    my ($result) = /->\s*([rp])/;
    $attr{$op} = { parms=>scalar @parms,
		   types=>$types,
		   func=>/\w+\(/?1:0,
		   opcode=>$opcode,
		   result=>$result
		 };
    print OUT "use constant RBC_\U$op\E => $opcode;\n";
    ++$opcode;
  }
  if (/^\#define RM_WORD_PACK \"(.)\"/) {
    $reg_pack = $1; 
  }
}
print OUT "\nour \@EXPORT = qw(@ops);\n\n";
# previously we used Data::Dumper, with Sortkeys() 
# to make sure the generated code only changed when the data
# changed.  Unfortunately Sortkeys isn't supported in some versions of
# perl we try to support, so we now generate this manually
print OUT "our %Attr =\n  (\n";
for my $opname (sort keys %attr) {
  my $op = $attr{$opname};
  print OUT "  '$opname' =>\n    {\n";
  for my $attrname (sort keys %$op) {
    my $attr = $op->{$attrname};
    print OUT "    '$attrname' => ";
    if (defined $attr) {
      if ($attr =~ /^\d+$/) {
	print OUT $attr;
      }
      else {
	print OUT "'$attr'";
      }
    }
    else {
      print OUT "undef";
    }

    print OUT ",\n";
  }
  print OUT "    },\n";
}
print OUT "  );\n";
print OUT "our \$MaxOperands = $max_opr;\n";
print OUT qq/our \$PackCode = "$reg_pack";\n/;
print OUT <<'EOS';
1;

__END__

=head1 NAME

Imager::Regops - generated information about the register based virtual machine

=head1 SYNOPSIS

  use Imager::Regops;
  $Imager::Regops::Attr{$opname}->{opcode} # opcode for given operator
  $Imager::Regops::Attr{$opname}->{parms} # number of parameters
  $Imager::Regops::Attr{$opname}->{types} # types of parameters
  $Imager::Regops::Attr{$opname}->{func} # operator is a function
  $Imager::Regops::Attr{$opname}->{result} # r for numeric, p for pixel result
  $Imager::Regops::MaxOperands; # maximum number of operands

=head1 DESCRIPTION

This module is generated automatically from F<regmach.h> so we don't need to
maintain the same information in at least one extra place.

At least that's the idea.

=head1 AUTHOR

Tony Cook, tony@develop-help.com

=head1 SEE ALSO

perl(1), Imager(3), http://imager.perl.org/

=cut

EOS
close(OUT) or die "Cannot close $out: $!";
close IN;
