package Imager::Fill;
use strict;
# this needs to be kept in sync with the array of hatches in fills.c
my @hatch_types =
  qw/check1x1 check2x2 check4x4 vline1 vline2 vline4
     hline1 hline2 hline4 slash1 slosh1 slash2 slosh2
     grid1 grid2 grid4 dots1 dots4 dots16 stipple weave cross1 cross2
     vlozenge hlozenge scalesdown scalesup scalesleft scalesright stipple2
     tile_L stipple3/;
my %hatch_types;
@hatch_types{@hatch_types} = 0..$#hatch_types;

my @combine_types = 
  qw/none normal multiply dissolve add subtract diff lighten darken
     hue saturation value color/;
my %combine_types;
@combine_types{@combine_types} = 0 .. $#combine_types;
$combine_types{mult} = $combine_types{multiply};
$combine_types{'sub'}  = $combine_types{subtract};
$combine_types{sat}  = $combine_types{saturation};

# this function tries to DWIM for color parameters
#  color objects are used as is
#  simple scalars are simply treated as single parameters to Imager::Color->new
#  hashrefs are treated as named argument lists to Imager::Color->new
#  arrayrefs are treated as list arguments to Imager::Color->new iff any
#    parameter is > 1
#  other arrayrefs are treated as list arguments to Imager::Color::Float

sub _color {
  my $arg = shift;
  my $result;

  if (ref $arg) {
    if (UNIVERSAL::isa($arg, "Imager::Color")
        || UNIVERSAL::isa($arg, "Imager::Color::Float")) {
      $result = $arg;
    }
    else {
      if ($arg =~ /^HASH\(/) {
        $result = Imager::Color->new(%$arg);
      }
      elsif ($arg =~ /^ARRAY\(/) {
        if (grep $_ > 1, @$arg) {
          $result = Imager::Color->new(@$arg);
        }
        else {
          $result = Imager::Color::Float->new(@$arg);
        }
      }
      else {
        $Imager::ERRSTR = "Not a color";
      }
    }
  }
  else {
    # assume Imager::Color::new knows how to handle it
    $result = Imager::Color->new($arg);
  }

  return $result;
}

sub new {
  my ($class, %hsh) = @_;

  my $self = bless { }, $class;
  $hsh{combine} ||= 0;
  if (exists $combine_types{$hsh{combine}}) {
    $hsh{combine} = $combine_types{$hsh{combine}};
  }
  if ($hsh{solid}) {
    my $solid = _color($hsh{solid});
    if (UNIVERSAL::isa($solid, 'Imager::Color')) {
      $self->{fill} = 
        Imager::i_new_fill_solid($solid, $hsh{combine});
    }
    elsif (UNIVERSAL::isa($solid, 'Imager::Color::Float')) {
      $self->{fill} = 
        Imager::i_new_fill_solidf($solid, $hsh{combine});
    }
    else {
      $Imager::ERRSTR = "solid isn't a color";
      return undef;
    }
  }
  elsif (defined $hsh{hatch}) {
    $hsh{dx} ||= 0;
    $hsh{dy} ||= 0;
    $hsh{fg} ||= Imager::Color->new(0, 0, 0);
    if (ref $hsh{hatch}) {
      $hsh{cust_hatch} = pack("C8", @{$hsh{hatch}});
      $hsh{hatch} = 0;
    }
    elsif ($hsh{hatch} =~ /\D/) {
      unless (exists($hatch_types{$hsh{hatch}})) {
        $Imager::ERRSTR = "Unknown hatch type $hsh{hatch}";
        return undef;
      }
      $hsh{hatch} = $hatch_types{$hsh{hatch}};
    }
    my $fg = _color($hsh{fg});
    if (UNIVERSAL::isa($fg, 'Imager::Color')) {
      my $bg = _color($hsh{bg} || Imager::Color->new(255, 255, 255));
      $self->{fill} = 
        Imager::i_new_fill_hatch($fg, $bg, $hsh{combine}, 
                                 $hsh{hatch}, $hsh{cust_hatch}, 
                                 $hsh{dx}, $hsh{dy});
    }
    elsif (UNIVERSAL::isa($fg, 'Imager::Color::Float')) {
      my $bg  = _color($hsh{bg} || Imager::Color::Float->new(1, 1, 1));
      $self->{fill} = 
        Imager::i_new_fill_hatchf($fg, $bg, $hsh{combine},
                                  $hsh{hatch}, $hsh{cust_hatch}, 
                                  $hsh{dx}, $hsh{dy});
    }
    else {
      $Imager::ERRSTR = "fg isn't a color";
      return undef;
    }
  }
  elsif (defined $hsh{fountain}) {
    # make sure we track the filter's defaults
    my $fount = $Imager::filters{fountain};
    my $def = $fount->{defaults};
    my $names = $fount->{names};
    
    $hsh{ftype} = $hsh{fountain};
    # process names of values
    for my $name (keys %$names) {
      if (defined $hsh{$name} && exists $names->{$name}{$hsh{$name}}) {
        $hsh{$name} = $names->{$name}{$hsh{$name}};
      }
    }
    # process defaults
    %hsh = (%$def, %hsh);
    my @parms = @{$fount->{callseq}};
    shift @parms;
    for my $name (@parms) {
      unless (defined $hsh{$name}) {
        $Imager::ERRSTR = 
          "required parameter '$name' not set for fountain fill";
        return undef;
      }
    }

    $self->{fill} =
      Imager::i_new_fill_fount($hsh{xa}, $hsh{ya}, $hsh{xb}, $hsh{yb},
                  $hsh{ftype}, $hsh{repeat}, $hsh{combine}, $hsh{super_sample},
                  $hsh{ssample_param}, $hsh{segments});
  }
  elsif (defined $hsh{image}) {
    $hsh{xoff} ||= 0;
    $hsh{yoff} ||= 0;
    $self->{fill} =
      Imager::i_new_fill_image($hsh{image}{IMG}, $hsh{matrix}, $hsh{xoff}, 
                               $hsh{yoff}, $hsh{combine});
    $self->{DEPS} = [ $hsh{image}{IMG} ];
  }
  else {
    $Imager::ERRSTR = "No fill type specified";
    warn "No fill type!";
    return undef;
  }

  $self;
}

sub hatches {
  return @hatch_types;
}

sub combines {
  return @combine_types;
}

1;

=head1 NAME

  Imager::Fill - general fill types

=head1 SYNOPSIS

  my $fill1 = Imager::Fill->new(solid=>$color, combine=>$combine);
  my $fill2 = Imager::Fill->new(hatch=>'vline2', fg=>$color1, bg=>$color2,
                                dx=>$dx, dy=>$dy);
  my $fill3 = Imager::Fill->new(fountain=>$type, ...);
  my $fill4 = Imager::Fill->new(image=>$img, ...);

=head1 DESCRIPTION 

Creates fill objects for use by some drawing functions, currently just
the Imager box() method.

The currently available fills are:

=over

=item *

solid

=item *

hatch

=item

fountain (similar to gradients in paint software)

=back

=head1 Common options

=over

=item combine

The way in which the fill data is combined with the underlying image,
possible values include:

=over

=item none

The fill pixel replaces the target pixel.

=item normal

The fill pixels alpha value is used to combine it with the target pixel.

=item multiply

=item mult

Each channel of fill and target is multiplied, and the result is
combined using the alpha channel of the fill pixel.

=item dissolve

If the alpha of the fill pixel is greater than a random number, the
fill pixel is alpha combined with the target pixel.

=item add

The channels of the fill and target are added together, clamped to the range of the samples and alpha combined with the target.

=item subtract

The channels of the fill are subtracted from the target, clamped to be
>= 0, and alpha combined with the target.

=item diff

The channels of the fill are subtracted from the target and the
absolute value taken this is alpha combined with the target.

=item lighten

The higher value is taken from each channel of the fill and target pixels, which is then alpha combined with the target.

=item darken

The higher value is taken from each channel of the fill and target pixels, which is then alpha combined with the target.

=item hue

The combination of the saturation and value of the target is combined
with the hue of the fill pixel, and is then alpha combined with the
target.

=item sat

The combination of the hue and value of the target is combined
with the saturation of the fill pixel, and is then alpha combined with the
target.

=item value

The combination of the hue and value of the target is combined
with the value of the fill pixel, and is then alpha combined with the
target.

=item color

The combination of the value of the target is combined with the hue
and saturation of the fill pixel, and is then alpha combined with the
target.

=back

=back

In general colors can be specified as Imager::Color or
Imager::Color::Float objects.  The fill object will typically store
both types and convert from one to the other.  If a fill takes 2 color
objects they should have the same type.

=head2 Solid fills

  my $fill = Imager::Fill->new(solid=>$color, $combine =>$combine)

Creates a solid fill, the only required parameter is C<solid> which
should be the color to fill with.

=head2 Hatched fills

  my $fill = Imager::Fill->new(hatch=>$type, fg=>$fgcolor, bg=>$bgcolor,
                               dx=>$dx, $dy=>$dy);

Creates a hatched fill.  You can specify the following keywords:

=over

=item hatch

The type of hatch to perform, this can either be the numeric index of
the hatch (not recommended), the symbolic name of the hatch, or an
array of 8 integers which specify the pattern of the hatch.

Hatches are represented as cells 8x8 arrays of bits, which limits their
complexity.

Current hatch names are:

=over

=item check1x1, check2x2, check4x4

checkerboards at varios sizes

=item vline1, vline2, vline4

1, 2, or 4 vertical lines per cell

=item hline1, hline2, hline4

1, 2, or 4 horizontal lines per cell

=item slash1,  slash2

1 or 2 / lines per cell.

=item slosh1,  slosh2

1 or 2 \ lines per cell

=item grid1,  grid2,  grid4

1, 2, or 4 vertical and horizontal lines per cell

=item dots1, dots4, dots16

1, 4 or 16 dots per cell

=item stipple, stipple2

see the samples

=item weave

I hope this one is obvious.

=item cross1,  cross2

2 densities of crosshatch

=item vlozenge,  hlozenge

something like lozenge tiles

=item scalesdown,  scalesup,  scalesleft,  scalesright

Vaguely like fish scales in each direction.

=item tile_L

L-shaped tiles

=back

=item fg

=item bg

The fg color is rendered where bits are set in the hatch, and the bg
where they are clear.  If you use a transparent fg or bg, and set
combine, you can overlay the hatch onto an existing image.

fg defaults to black, bg to white.

=item dx

=item dy

An offset into the hatch cell.  Both default to zero.

=back

You can call Imager::Fill->hatches for a list of hatch names.

=head2 Fountain fills

  my $fill = Imager::Fill->new(fountain=>$ftype, 
       xa=>$xa, ya=>$ya, xb=>$xb, yb=>$yb, 
       segment=>$segments, repeat=>$repeat, combine=>$combine, 
       super_sample=>$super_sample, ssample_param=>$ssample_param);

This fills the given region with a fountain fill.  This is exactly the
same fill as the C<fountain> filter, but is restricted to the shape
you are drawing, and the fountain parameter supplies the fill type,
and is required.

=head2 Image Fills

  my $fill = Imager::Fill->new(image=>$src, xoff=>$xoff, yoff=>$yoff,
                               matrix=>$matrix, $combine);

Fills the given image with a tiled version of the given image.  The
first non-zero value of xoff or yoff will provide an offset along the
given axis between rows or columns of tiles respectively.

The matrix parameter performs a co-ordinate transformation from the
co-ordinates in the target image to the fill image co-ordinates.
Linear interpolation is used to determine the fill pixel.  You can use
the L<Imager::Matrix2d> class to create transformation matrices.

The matrix parameter will significantly slow down the fill.

=head1 OTHER METHODS

=over

=item Imager::Fill->hatches

A list of all defined hatch names.

=item Imager::Fill->combines

A list of all combine types.

=back

=head1 FUTURE PLANS

I'm planning on adding the following types of fills:

=over

=item checkerboard

combines 2 other fills in a checkerboard

=item combine

combines 2 other fills using the levels of an image

=item regmach

uses the transform2() register machine to create fills

=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3)

=cut
