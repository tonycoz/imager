package Imager::Fill;

# this needs to be kept in sync with the array of hatches in fills.c
my @hatch_types =
  qw/check1x1 check2x2 check4x4 vline1 vline2 vline4
     hline1 hline2 hline4 slash1 slosh1 slash2 slosh2
     grid1 grid2 grid4 dots1 dots4 dots16 stipple weave cross1 cross2
     vlozenge hlozenge scalesdown scalesup scalesleft scalesright stipple2
     tile_L/;
my %hatch_types;
@hatch_types{@hatch_types} = 0..$#hatch_types;

sub new {
  my ($class, %hsh) = @_;

  my $self = bless { }, $class;
  $hsh{combine} ||= 0;
  if ($hsh{solid}) {
    if (UNIVERSAL::isa($hsh{solid}, 'Imager::Color')) {
      $self->{fill} = Imager::i_new_fill_solid($hsh{solid}, $hsh{combine});
    }
    elsif (UNIVERSAL::isa($hsh{colid}, 'Imager::Color::Float')) {
      $self->{fill} = Imager::i_new_fill_solidf($hsh{solid}, $hsh{combine});
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
    if (UNIVERSAL::isa($hsh{fg}, 'Imager::Color')) {
      $hsh{bg} ||= Imager::Color->new(255, 255, 255);
      $self->{fill} = 
        Imager::i_new_fill_hatch($hsh{fg}, $hsh{bg}, $hsh{combine}, 
                                 $hsh{hatch}, $hsh{cust_hatch}, 
                                 $hsh{dx}, $hsh{dy});
    }
    elsif (UNIVERSAL::isa($hsh{bg}, 'Imager::Color::Float')) {
      $hsh{bg} ||= Imager::Color::Float->new(1, 1, 1);
      $self->{fill} = 
        Imager::i_new_fill_hatchf($hsh{fg}, $hsh{bg}, $hsh{combine},
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

1;

=head1 NAME

  Imager::Fill - general fill types

=head1 SYNOPSIS

  my $fill1 = Imager::Fill->new(solid=>$color, combine=>$combine);
  my $fill2 = Imager::Fill->new(hatch=>'vline2', fg=>$color1, bg=>$color2,
                                dx=>$dx, dy=>$dy);

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

If this is non-zero the fill combines the given colors or samples (if
the fill is an image) with the underlying image.

This this is missing or zero then the target image pixels are simply
overwritten.

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

=head1 FUTURE PLANS

I'm planning on adding the following types of fills:

=over

=item image

tiled image fill

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
