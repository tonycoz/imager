package Imager::Font;

use Imager::Color;
use strict;
use vars qw(%T1_Paths %TT_Paths %T1_Cache %TT_Cache $TT_CSize $TT_CSize $T1AA);

# This class is a container
# and works for both truetype and t1 fonts.


# $T1AA is in there because for some reason (probably cache related) antialiasing
# is a system wide setting in t1 lib.

sub t1_set_aa_level {
  if (!defined $T1AA or $_[0] != $T1AA) {
    Imager::i_t1_set_aa($_[0]);
    $T1AA=$_[0];
  }
}

# search method
# 1. start by checking if file is the parameter
# 1a. if so qualify path and compare to the cache.
# 2a. if in cache - take it's id from there and increment count.
#

sub new {
  my $class = shift;
  my $self ={};
  my ($file,$type,$id);
  my %hsh=(color=>Imager::Color->new(255,0,0,0),
	   size=>15,
	   @_);

  bless $self,$class;

  if ($hsh{'file'}) { 
    $file=$hsh{'file'};
    if ( $file !~ m/^\// ) {
      $file='./'.$file;
      if (! -e $file) {
	$Imager::ERRSTR="Font $file not found";
	return();
      }
    }

    $type=$hsh{'type'};
    if (!defined($type) or $type !~ m/^(t1|tt)/) {
      $type='tt' if $file =~ m/\.ttf$/i;
      $type='t1' if $file =~ m/\.pfb$/i;
    }
    if (!defined($type)) {
      $Imager::ERRSTR="Font type not found";
      return;
    }
  } else {
    $Imager::ERRSTR="No font file specified";
    return;
  }

  if (!$Imager::formats{$type}) { 
    $Imager::ERRSTR="`$type' not enabled";
    return;
  }

  # here we should have the font type or be dead already.

  if ($type eq 't1') {
    $id=Imager::i_t1_new($file);
  }

  if ($type eq 'tt') {
    $id=Imager::i_tt_new($file);
  }

  $self->{'aa'}=$hsh{'aa'}||'0';
  $self->{'file'}=$file;
  $self->{'id'}=$id;
  $self->{'type'}=$type;
  $self->{'size'}=$hsh{'size'};
  $self->{'color'}=$hsh{'color'};
  return $self;
}






sub bounding_box {
  my $self=shift;
  my %input=@_;
  my @box;

  if (!exists $input{'string'}) { $Imager::ERRSTR='string parameter missing'; return; }

  if ($self->{type} eq 't1') {
    @box=Imager::i_t1_bbox($self->{id}, defined($input{size}) ? $input{size} :$self->{size},
			   $input{string}, length($input{string}));
  }
  if ($self->{type} eq 'tt') {
    @box=Imager::i_tt_bbox($self->{id}, defined($input{size}) ? $input{size} :$self->{size},
			   $input{string}, length($input{string}));
  }

  if(exists $input{'x'} and exists $input{'y'}) {
    my($gdescent, $gascent)=@box[1,3];
    $box[1]=$input{'y'}-$gascent;      # top = base - ascent (Y is down)
    $box[3]=$input{'y'}-$gdescent;     # bottom = base - descent (Y is down, descent is negative)
    $box[0]+=$input{'x'};
    $box[2]+=$input{'x'};
  } elsif (exists $input{'canon'}) {
    $box[3]-=$box[1];    # make it cannoical (ie (0,0) - (width, height))
    $box[2]-=$box[0];
  }
  return @box;
}

1;



__END__

=head1 NAME

Imager::Font - Font handling for Imager.

=head1 SYNOPSIS

  $t1font = Imager::Font->new(file => 'pathtofont.pfb');
  $ttfont = Imager::Font->new(file => 'pathtofont.ttf');

  $blue = Imager::Color->new("#0000FF");
  $font = Imager::Font->new(file  => 'pathtofont.ttf',
			    color => $blue,
			    size  => 30);

  ($neg_width,
   $global_descent,
   $pos_width,
   $global_ascent,
   $descent,
   $ascent) = $font->bounding_box(string=>"Foo");

  $logo = $font->logo(text   => "Slartibartfast Enterprises",
		      size   => 40,
		      border => 5,
		      color  => $green);
  # logo is proposed - doesn't exist yet


  $img->string(font  => $font,
	     text  => "Model-XYZ",
	     x     => 15,
	     y     => 40,
	     size  => 40,
	     color => $red
	     aa    => 1);

  # Documentation in Imager.pm

=head1 DESCRIPTION

This module handles creating Font objects used by imager.  The module
also handles querying fonts for sizes and such.  If both T1lib and
freetype were avaliable at the time of compilation then Imager should
be able to work with both truetype fonts and t1 postscript fonts.  To
check if Imager is t1 or truetype capable you can use something like
this:

  use Imager;
  print "Has truetype"      if $Imager::formats{tt};
  print "Has t1 postscript" if $Imager::formats{t1};


=over 4

=item new

This creates a font object to pass to functions that take a font argument.

  $font = Imager::Font->new(file  => 'denmark.ttf',
			    color => $blue,
			    size  => 30,
			    aa    => 1);

This creates a font which is the truetype font denmark.ttf.  It's
default color is $blue, default size is 30 pixels and it's rendered
antialised by default.  Imager can see which type of font a file is by
looking at the suffix of the filename for the font.  A suffix of 'ttf'
is taken to mean a truetype font while a suffix of 'pfb' is taken to
mean a t1 postscript font.  If Imager cannot tell which type a font is
you can tell it explicitly by using the C<type> parameter:

  $t1font = Imager::Font->new(file => 'fruitcase', type => 't1');
  $ttfont = Imager::Font->new(file => 'arglebarf', type => 'tt');

If any of the C<color>, C<size> or C<aa> parameters are omitted when
calling C<Imager::Font->new()> the they take the following values:


  color => Imager::Color->new(255, 0, 0, 0);  # this default should be changed
  size  => 15
  aa    => 0

=item bounding_box
Returns the bounding box for the specified string.  Example:

  ($neg_width,
   $global_descent,
   $pos_width,
   $global_ascent,
   $descent,
   $ascent) = $font->bounding_box(string => "A Fool");

The C<$neg_width> is the relative start of a the string.  In some
cases this can be a negative number, in that case the first letter
stretches to the left of the starting position that is specified in
the string method of the Imager class.  <$global_descent> is the how
far down the lowest letter of the entire font reaches below the
baseline (this is often j).  C<$pos_width> is how wide the string from
from the starting position is.  The total width of the string is
C<$pos_width-$neg_width>.  C<$descent> and C<$ascent> are the as
<$global_descent> and <$global_ascent> except that they are only for
the characters that appear in the string.  Obviously we can stuff all
the results into an array just as well:

  @metrics = $font->bounding_box(string => "testing 123");

Note that extra values may be added, so $metrics[-1] isn't supported.
It's possible to translate the output by a passing coordinate to the
bounding box method:

  @metrics = $font->bounding_box(string => "testing 123", x=>45, y=>34);

This gives the bounding box as if the string had been put down at C<(x,y)>
By giving bounding_box 'canon' as a true value it's possible to measure
the space needed for the string:

  @metrics = $font->bounding_box(string=>"testing",size=>15,canon=>1);

This returns tha same values in $metrics[0] and $metrics[1],
but:

 $bbox[2] - horizontal space taken by glyphs
 $bbox[3] - vertical space taken by glyphs



=item string

This is a method of the Imager class but because it's described in
here since it belongs to the font routines.  Example:

  $img=Imager->new();
  $img=read(file=>"test.jpg");
  $img->string(font=>$t1font,
               text=>"Model-XYZ",
               x=>0,
               y=>40,
               size=>40,
               color=>$red);
  $img->write(file=>"testout.jpg");

This would put a 40 pixel high text in the top left corner of an
image.  If you measure the actuall pixels it varies since the fonts
usually do not use their full height.  It seems that the color and
size can be specified twice.  When a font is created only the actual
font specified matters.  It his however convenient to store default
values in a font, such as color and size.  If parameters are passed to
the string function they are used instead of the defaults stored in
the font.

If string() is called with the C<channel> parameter then the color
isn't used and the font is drawn in only one channel of the image.
This can be quite handy to create overlays.  See the examples for tips
about this.

Sometimes it is necessary to know how much space a string takes before
rendering it.  The bounding_box() method described earlier can be used
for that.


=item logo

This method doesn't exist yet but is under consideration.  It would mostly
be helpful for generating small tests and such.  Its proposed interface is:

  $img = $font->logo(string=>"Plan XYZ", color=>$blue, border=>7);

This would be nice for writing (admittedly multiline) one liners like:

Imager::Font->new(file=>"arial.ttf", color=>$blue, aa=>1)
            ->string(text=>"Plan XYZ", border=>5)
            ->write(file=>"xyz.png");



=back

=head1 AUTHOR

Arnar M. Hrafnkelsson, addi@umich.edu
And a great deal of help from others - see the README for a complete
list.

=head1 SEE ALSO

Imager(3)
http://www.eecs.umich.edu/~addi/perl/Imager/

=cut


