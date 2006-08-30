#!perl -w
use strict;

my $src = shift;
my $dest = shift
  or usage();

open SRC, "< $src"
  or die "Cannot open $src: $!\n";

my $cond;
my $cond_line;
my $save_code;
my @code;
my $code_line;
my @out;
my $failed;

push @out, "#line 1 \"$src\"\n";
while (defined(my $line = <SRC>)) {
  if ($line =~ /^\#code\s+(\S.+)$/) {
    $save_code
      and do { warn "$src:$code_line:Unclosed #code block\n"; ++$failed; };

    $cond = $1;
    $cond_line = $.;
    $code_line = $. + 1;
    $save_code = 1;
  }
  elsif ($line =~ /^\#code\s*$/) {
    $save_code
      and do { warn "$src:$code_line:Unclosed #code block\n"; ++$failed; };

    $cond = '';
    $cond_line = 0;
    $code_line = $. + 1;
    $save_code = 1;
  }
  elsif ($line =~ /^\#\/code$/) {
    $save_code
      or do { warn "$src:$.:#/code without #code\n"; ++$failed; next; };

    if ($cond) {
      push @out, "#line $cond_line \"$src\"\n";
      push @out, "  if ($cond) {\n";
    }
    push @out, "#undef IM_EIGHT_BIT\n",
      "#define IM_EIGHT_BIT 1\n";
    push @out, "#line $code_line \"$src\"\n";
    push @out, byte_samples(@code);
    push @out, "  }\n", "  else {\n"
      if $cond;
    push @out, "#undef IM_EIGHT_BIT\n";
    push @out, "#line $code_line \"$src\"\n";
    push @out, double_samples(@code);
    push @out, "  }\n"
      if $cond;
    push @out, "#line $. \"$src\"\n";
    @code = ();
    $save_code = 0;
  }
  elsif ($save_code) {
    push @code, $line;
  }
  else {
    push @out, $line;
  }
}

if ($save_code) {
  warn "$src:$code_line:#code block not closed by EOF\n";
  ++$failed;
}

close SRC;

$failed 
  and die "Errors during parsing, aborting\n";

open DEST, "> $dest"
  or die "Cannot open $dest: $!\n";
print DEST @out;
close DEST;

sub byte_samples {
  # important we make a copy
  my @lines = @_;

  for (@lines) {
    s/\bIM_GPIX\b/i_gpix/g;
    s/\bIM_GLIN\b/i_glin/g;
    s/\bIM_PPIX\b/i_ppix/g;
    s/\bIM_PLIN\b/i_plin/g;
    s/\bIM_GSAMP\b/i_gsamp/g;
    s/\bIM_SAMPLE_MAX\b/255/g;
    s/\bIM_SAMPLE_T/i_sample_t/g;
    s/\bIM_COLOR\b/i_color/g;
    s/\bIM_WORK_T\b/int/g;
    s/\bIM_Sf\b/"%d"/g;
    s/\bIM_Wf\b/"%d"/g;
    s/\bIM_SUFFIX\((\w+)\)/$1_8/g;
  }

  @lines;
}

sub double_samples {
  # important we make a copy
  my @lines = @_;

  for (@lines) {
    s/\bIM_GPIX\b/i_gpixf/g;
    s/\bIM_GLIN\b/i_glinf/g;
    s/\bIM_PPIX\b/i_ppixf/g;
    s/\bIM_PLIN\b/i_plinf/g;
    s/\bIM_GSAMP\b/i_gsampf/g;
    s/\bIM_SAMPLE_MAX\b/1.0/g;
    s/\bIM_SAMPLE_T/i_fsample_t/g;
    s/\bIM_COLOR\b/i_fcolor/g;
    s/\bIM_WORK_T\b/double/g;
    s/\bIM_Sf\b/"%f"/g;
    s/\bIM_Wf\b/"%f"/g;
    s/\bIM_SUFFIX\((\w+)\)/$1_double/g;
  }

  @lines;
}

=head1 NAME

imtoc.perl - simple preprocessor for handling multiple sample sizes

=head1 SYNOPSIS

  /* in the source: */
  #code condition true to work with 8-bit samples
  ... code using preprocessor types/values ...
  #/code

  perl imtoc.perl foo.im foo.c

=head1 DESCRIPTION

This is a simple preprocessor that aims to reduce duplication of
source code when implementing an algorithm both for 8-bit samples and
double samples in Imager.

Imager's Makefile.PL currently scans the MANIFEST for .im files and
adds Makefile files to convert these to .c files.

The beginning of a sample-independent section of code is preceded by:

  #code expression

where I<expression> should return true if processing should be done at
8-bits/sample.

You can also use a #code block around a function definition to produce
8-bit and double sample versions of a function.  In this case #code
has no expression and you will need to use IM_SUFFIX() to produce
different function names.

The end of a sample-independent section of code is terminated by:

  #/code

#code sections cannot be nested.

#/code without a starting #code is an error.

The following types and values are defined in a #code section:

=over

=item *

IM_GPIX(im, x, y, &col)

=item *

IM_GLIN(im, l, r, y, colors)

=item *

IM_PPIX(im, x, y, &col)

=item *

IM_PLIN(im, x, y, colors)

=item *

IM_GSAMP(im, l, r, y, samples, chans, chan_count)

These correspond to the appropriate image function, eg. IM_GPIX()
becomes i_gpix() or i_gpixf() as appropriate.

=item *

IM_SAMPLE_MAX - maximum value for a sample

=item *

IM_SAMPLE_T - type of a sample (i_sample_t or i_fsample_t)

=item *

IM_COLOR - color type, either i_color or i_fcolor.

=item *

IM_WORK_T - working sample type, either int or double.

=item *

IM_Sf - format string for the sample type, C<"%d"> or C<"%f">.

=item *

IM_Wf - format string for the work type, C<"%d"> or C<"%f">.

=item *

IM_SUFFIX(identifier) - adds _8 or _double onto the end of identifier.

=item *

IM_EIGHT_BIT - this is a macro defined only in 8-bit/sample code.

=back

Other types, functions and values may be added in the future.

=head1 AUTHOR

Tony Cook <tony@imager.perl.org>

=cut
