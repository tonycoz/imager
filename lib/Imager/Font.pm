package Imager::Font;

use Imager::Color;
use strict;

# the aim here is that we can:
#  - add file based types in one place: here
#  - make sure we only attempt to create types that exist
#  - give reasonable defaults
#  - give the user some control over which types get used
my %drivers =
  (
   tt=>{
        class=>'Imager::Font::Truetype',
        module=>'Imager/Font/Truetype.pm',
        files=>'.*\.ttf$',
       },
   t1=>{
        class=>'Imager::Font::Type1',
        module=>'Imager/Font/Type1.pm',
        files=>'.*\.pfb$',
       },
   ft2=>{
         class=>'Imager::Font::FreeType2',
         module=>'Imager/Font/FreeType2.pm',
         files=>'.*\.(pfa|pfb|otf|ttf|fon|fnt)$',
        },
   ifs=>{
         class=>'Imager::Font::Image',
         module=>'Imager/Font/Image.pm',
         files=>'.*\.ifs$',
        },
   w32=>{
         class=>'Imager::Font::Win32',
         module=>'Imager/Font/Win32.pm',
        },
  );

# this currently should only contain file based types, don't add w32
my @priority = qw(t1 tt ft2 ifs);

# when Imager::Font is loaded, Imager.xs has not been bootstrapped yet
# this function is called from Imager.pm to finish initialization
sub __init {
  @priority = grep Imager::i_has_format($_), @priority;
  delete @drivers{grep !Imager::i_has_format($_), keys %drivers};
}

# search method
# 1. start by checking if file is the parameter
# 1a. if so qualify path and compare to the cache.
# 2a. if in cache - take it's id from there and increment count.
#

sub new {
  my $class = shift;
  my $self = {};
  my ($file, $type, $id);
  my %hsh=(color => Imager::Color->new(255,0,0,0),
	   size => 15,
	   @_);

  bless $self,$class;

  if ($hsh{'file'}) {
    $file = $hsh{'file'};
    if ( $file !~ m/^\// ) {
      $file = './'.$file;
      if (! -e $file) {
	$Imager::ERRSTR = "Font $file not found";
	return();
      }
    }

    $type = $hsh{'type'};
    if (!defined($type) or !$drivers{$type}) {
      for my $drv (@priority) {
        undef $type;
        my $re = $drivers{$drv}{files} or next;
        if ($file =~ /$re/i) {
          $type = $drv;
          last;
        }
      }
    }
    if (!defined($type)) {
      $Imager::ERRSTR = "Font type not found";
      return;
    }
  } elsif ($hsh{face}) {
    $type = "w32";
  } else {
    $Imager::ERRSTR="No font file specified";
    return;
  }

  if (!$Imager::formats{$type}) {
    $Imager::ERRSTR = "`$type' not enabled";
    return;
  }

  # here we should have the font type or be dead already.

  require $drivers{$type}{module};
  return $drivers{$type}{class}->new(%hsh);
}

# returns first defined parameter
sub _first {
  for (@_) {
    return $_ if defined $_;
  }
  return undef;
}

sub draw {
  my $self = shift;
  my %input = ('x' => 0, 'y' => 0, @_);
  unless ($input{image}) {
    $Imager::ERRSTR = 'No image supplied to $font->draw()';
    return;
  }
  $input{string} = _first($input{string}, $input{text});
  unless (defined $input{string}) {
    $Imager::ERRSTR = "Missing required parameter 'string'";
    return;
  }
  $input{aa} = _first($input{aa}, $input{antialias}, $self->{aa}, 1);
  # the original draw code worked this out but didn't use it
  $input{align} = _first($input{align}, $self->{align});
  $input{color} = _first($input{color}, $self->{color});
  $input{color} = Imager::_color($input{'color'});

  $input{size} = _first($input{size}, $self->{size});
  unless (defined $input{size}) {
    $input{image}{ERRSTR} = "No font size provided";
    return undef;
  }
  $input{align} = _first($input{align}, 1);
  $input{utf8} = _first($input{utf8}, $self->{utf8}, 0);
  $input{vlayout} = _first($input{vlayout}, $self->{vlayout}, 0);

  $self->_draw(%input);
}

sub align {
  my $self = shift;
  my %input = ( halign => 'left', valign => 'baseline', 
                'x' => 0, 'y' => 0, @_ );

  my $text = _first($input{string}, $input{text});
  unless (defined $text) {
    Imager->_set_error("Missing required parameter 'string'");
    return;
  }

  # image needs to be supplied, but can be supplied as undef
  unless (exists $input{image}) {
    Imager->_set_error("Missing required parameter 'image'");
    return;
  }
  my $size = _first($input{size}, $self->{size});
  my $utf8 = _first($input{utf8}, 0);

  my $bbox = $self->bounding_box(string=>$text, size=>$size, utf8=>$utf8);
  my $valign = $input{valign};
  $valign = 'baseline'
    unless $valign && $valign =~ /^(?:top|center|bottom|baseline)$/;

  my $halign = $input{halign};
  $halign = 'start' 
    unless $halign && $halign =~ /^(?:left|start|center|end|right)$/;

  my $x = $input{'x'};
  my $y = $input{'y'};

  if ($valign eq 'top') {
    $y += $bbox->ascent;
  }
  elsif ($valign eq 'center') {
    $y += $bbox->ascent - $bbox->text_height / 2;
  }
  elsif ($valign eq 'bottom') {
    $y += $bbox->descent;
  }
  # else baseline is the default

  if ($halign eq 'left') {
    $x -= $bbox->start_offset;
  }
  elsif ($halign eq 'start') {
    # nothing to do
  }
  elsif ($halign eq 'center') {
    $x -= $bbox->start_offset + $bbox->total_width / 2;
  }
  elsif ($halign eq 'end' || $halign eq 'right') {
    $x -= $bbox->start_offset + $bbox->total_width - 1;
  }
  $x = int($x);
  $y = int($y);

  if ($input{image}) {
    delete @input{qw/x y/};
    $self->draw(%input, 'x' => $x, 'y' => $y, align=>1)
      or return;
#      for my $i (1 .. length $text) {
#        my $work = substr($text, 0, $i);
#        my $bbox = $self->bounding_box(string=>$work, size=>$size, utf8=>$utf8);
#        my $nx = $x + $bbox->end_offset;
#        $input{image}->setpixel(x=>[ ($nx) x 5 ],
#                                'y'=>[ $y-2, $y-1, $y, $y+1, $y+2 ],
#                                color=>'FF0000');
#      }
  }

  return ($x+$bbox->start_offset, $y-$bbox->ascent, 
          $x+$bbox->end_offset, $y-$bbox->descent+1);
}

sub bounding_box {
  my $self=shift;
  my %input=@_;

  if (!exists $input{'string'}) { 
    $Imager::ERRSTR='string parameter missing'; 
    return;
  }
  $input{size} ||= $self->{size};
  $input{sizew} = _first($input{sizew}, $self->{sizew}, 0);
  $input{utf8} = _first($input{utf8}, $self->{utf8}, 0);

  my @box = $self->_bounding_box(%input);

  if (wantarray) {
    if(@box && exists $input{'x'} and exists $input{'y'}) {
      my($gdescent, $gascent)=@box[1,3];
      $box[1]=$input{'y'}-$gascent;      # top = base - ascent (Y is down)
      $box[3]=$input{'y'}-$gdescent;     # bottom = base - descent (Y is down, descent is negative)
      $box[0]+=$input{'x'};
      $box[2]+=$input{'x'};
    } elsif (@box && $input{'canon'}) {
      $box[3]-=$box[1];    # make it cannoical (ie (0,0) - (width, height))
      $box[2]-=$box[0];
    }
    return @box;
  }
  else {
    require Imager::Font::BBox;

    return Imager::Font::BBox->new(@box);
  }
}

sub dpi {
  my $self = shift;

  # I'm assuming a default of 72 dpi
  my @old = (72, 72);
  if (@_) {
    $Imager::ERRSTR = "Setting dpi not implemented for this font type";
    return;
  }

  return @old;
}

sub transform {
  my $self = shift;

  my %hsh = @_;

  # this is split into transform() and _transform() so we can 
  # implement other tags like: degrees=>12, which would build a
  # 12 degree rotation matrix
  # but I'll do that later
  unless ($hsh{matrix}) {
    $Imager::ERRSTR = "You need to supply a matrix";
    return;
  }

  return $self->_transform(%hsh);
}

sub _transform {
  $Imager::ERRSTR = "This type of font cannot be transformed";
  return;
}

sub utf8 {
  return 0;
}

sub priorities {
  my $self = shift;
  my @old = @priority;

  if (@_) {
    @priority = grep Imager::i_has_format($_), @_;
  }
  return @old;
}

1;

__END__

=head1 NAME

Imager::Font - Font handling for Imager.

=head1 SYNOPSIS

  $t1font = Imager::Font->new(file => 'pathtofont.pfb');
  $ttfont = Imager::Font->new(file => 'pathtofont.ttf');
  $w32font = Imager::Font->new(face => 'Times New Roman');

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
	     color => $red,
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
  print "Has Win32 fonts"   if $Imager::formats{w32};
  print "Has Freetype2"     if $Imager::formats{ft2};

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

To use Win32 fonts supply the facename of the font:

  $font = Imager::Font->new(face=>'Arial Bold Italic');

There isn't any access to other logical font attributes, but this
typically isn't necessary for Win32 TrueType fonts, since you can
contruct the full name of the font as above.

Other logical font attributes may be added if there is sufficient demand.

=item bounding_box

Returns the bounding box for the specified string.  Example:

  my ($neg_width,
      $global_descent,
      $pos_width,
      $global_ascent,
      $descent,
      $ascent,
      $advance_width) = $font->bounding_box(string => "A Fool");

  my $bbox_object = $font->bounding_box(string => "A Fool");

=over

=item C<$neg_width>

the relative start of a the string.  In some
cases this can be a negative number, in that case the first letter
stretches to the left of the starting position that is specified in
the string method of the Imager class

=item C<$global_descent> 

how far down the lowest letter of the entire font reaches below the
baseline (this is often j).

=item C<$pos_width>

how wide the string from
the starting position is.  The total width of the string is
C<$pos_width-$neg_width>.

=item C<$descent> 

=item C<$ascent> 

the same as <$global_descent> and <$global_ascent> except that they
are only for the characters that appear in the string.

=item C<$advance_width>

the distance from the start point that the next string output should
start at, this is often the same as C<$pos_width>, but can be
different if the final character overlaps the right side of its
character cell.

=back

Obviously we can stuff all the results into an array just as well:

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

Returns an L<Imager::Font::BBox> object in scalar context, so you can
avoid all those confusing indices.  This has methods as named above,
with some extra convenience methods.

=item string

This is a method of the Imager class but because it's described in
here since it belongs to the font routines.  Example:

  $img=Imager->new();
  $img->read(file=>"test.jpg");
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

The following parameters can be supplied to the string() method:

=over

=item string

The text to be rendered.  If this isn't present the 'text' parameter
is used.  If neither is present the call will fail.

=item aa

If non-zero the output will be anti-aliased.

=item x

=item y

The start point for rendering the text.  See the align parameter.

=item align

If non-zero the point supplied in (x,y) will be on the base-line, if
zero then (x,y) will be at the top-left of the first character.

=item channel

If present, the text will be written to the specified channel of the
image and the color parameter will be ignore.

=item color

The color to draw the text in.

=item size

The point-size to draw the text at.

=item sizew

For drivers that support it, the width to draw the text at.  Defaults
to be value of the 'size' parameter.

=item utf8

For drivers that support it, treat the string as UTF8 encoded.  For
versions of perl that support Unicode (5.6 and later), this will be
enabled automatically if the 'string' parameter is already a UTF8
string. See L<UTF8> for more information.

=item vlayout

For drivers that support it, draw the text vertically.  Note: I
haven't found a font that has the appropriate metrics yet.

=back

If string() is called with the C<channel> parameter then the color
isn't used and the font is drawn in only one channel of the image.
This can be quite handy to create overlays.  See the examples for tips
about this.

Sometimes it is necessary to know how much space a string takes before
rendering it.  The bounding_box() method described earlier can be used
for that.

=item align(string=>$text, size=>$size, x=>..., y=>..., valign => ..., halign=>...)

Higher level text output - outputs the text aligned as specified
around the given point (x,y).

  # "Hello" centered at 100, 100 in the image.
  my ($left, $top, $bottom, $right) = 
    $font->align(string=>"Hello",
                 x=>100, y=>100, 
                 halign=>'center', valign=>'center', 
                 image=>$image);

Takes the same parameters as $font->draw(), and the following extra
parameters:

=over

=item valign

Possible values are:

=over

=item top

Point is at the top of the text.

=item bottom

Point is at the bottom of the text.

=item baseline

Point is on the baseline of the text (default.)

=item center

Point is vertically centered within the text.

=back

=item halign

=over

=item left

The point is at the left of the text.

=item start

The point is at the start point of the text.

=item center

The point is horizontally centered within the text.

=item right

The point is at the right end of the text.

=item end

The point is at the right end of the text.  This will change to the
end point of the text (once the proper bounding box interfaces are
available).

=back

=item image

The image to draw to.  Set to C<undef> to avoid drawing but still
calculate the bounding box.

=back

Returns a list specifying the bounds of the drawn text.

=item dpi()

=item dpi(xdpi=>$xdpi, ydpi=>$ydpi)

=item dpi(dpi=>$dpi)

Set retrieve the spatial resolution of the image in dots per inch.
The default is 72 dpi.

This isn't implemented for all font types yet.

=item transform(matrix=>$matrix)

Applies a transformation to the font, where matrix is an array ref of
numbers representing a 2 x 3 matrix:

  [  $matrix->[0],  $matrix->[1],  $matrix->[2],
     $matrix->[3],  $matrix->[4],  $matrix->[5]   ]

Not all font types support transformations, these will return false.

It's possible that a driver will disable hinting if you use a
transformation, to prevent discontinuities in the transformations.
See the end of the test script t/t38ft2font.t for an example.

Currently only the ft2 (Freetype 2.x) driver supports the transform()
method.

=item has_chars(string=>$text)

Checks if the characters in $text are defined by the font.

In a list context returns a list of true or false value corresponding
to the characters in $text, true if the character is defined, false if
not.  In scalar context returns a string of NUL or non-NUL
characters.  Supports UTF8 where the font driver supports UTF8.

Not all fonts support this method (use $font->can("has_chars") to
check.)

=item logo

This method doesn't exist yet but is under consideration.  It would mostly
be helpful for generating small tests and such.  Its proposed interface is:

  $img = $font->logo(string=>"Plan XYZ", color=>$blue, border=>7);

This would be nice for writing (admittedly multiline) one liners like:

Imager::Font->new(file=>"arial.ttf", color=>$blue, aa=>1)
            ->string(text=>"Plan XYZ", border=>5)
            ->write(file=>"xyz.png");

=item face_name()

Returns the internal name of the face.  Not all font types support
this method yet.

=item glyph_names(string=>$string [, utf8=>$utf8 ][, reliable_only=>0 ] );

Returns a list of glyph names for each of the characters in the
string.  If the character has no name then C<undef> is returned for
the character.

Some font files do not include glyph names, in this case Freetype 2
will not return any names.  Freetype 1 can return standard names even
if there are no glyph names in the font.

Freetype 2 has an API function that returns true only if the font has
"reliable glyph names", unfortunately this always returns false for
TTF fonts.  This can avoid the check of this API by supplying
C<reliable_only> as 0.  The consequences of using this on an unknown
font may be unpredictable, since the Freetype documentation doesn't
say how those name tables are unreliable, or how FT2 handles them.

Both Freetype 1.x and 2.x allow support for glyph names to not be
included.

=back

=head1 UTF8

There are 2 ways of rendering Unicode characters with Imager:

=over

=item *

For versions of perl that support it, use perl's native UTF8 strings.
This is the simplest method.

=item *

Hand build your own UTF8 encoded strings.  Only recommended if your
version of perl has no UTF8 support.

=back

Imager won't construct characters for you, so if want to output
unicode character 00C3 "LATIN CAPITAL LETTER A WITH DIAERESIS", and
your font doesn't support it, Imager will I<not> build it from 0041
"LATIN CAPITAL LETTER A" and 0308 "COMBINING DIAERESIS".

=head2 Native UTF8 Support

If your version of perl supports UTF8 and the driver supports UTF8,
just use the $im->string() method, and it should do the right thing.

=head2 Build your own

In this case you need to build your own UTF8 encoded characters.

For example:

 $x = pack("C*", 0xE2, 0x80, 0x90); # character code 0x2010 HYPHEN

You need to be be careful with versions of perl that have UTF8
support, since your string may end up doubly UTF8 encoded.

For example:

 $x = "A\xE2\x80\x90\x41\x{2010}";
 substr($x, -1, 0) = ""; 
 # at this point $x is has the UTF8 flag set, but has 5 characters,
 # none, of which is the constructed UTF8 character

The test script t/t38ft2font.t has a small example of this after the 
comment:

  # an attempt using emulation of UTF8

=head1 DRIVER CONTROL

If you don't supply a 'type' parameter to Imager::Font->new(), but you
do supply a 'file' parameter, Imager will attempt to guess which font
driver to used based on the extension of the font file.

Since some formats can be handled by more than one driver, a priority
list is used to choose which one should be used, if a given format can
be handled by more than one driver.

The current priority can be retrieved with:

  @drivers = Imager::Font->priorities();

You can set new priorities and save the old priorities with:

  @old = Imager::Font->priorities(@drivers);

If you supply driver names that are not currently supported, they will
be ignored.

Imager supports both T1Lib and Freetype2 for working with Type 1
fonts, but currently only T1Lib does any caching, so by default T1Lib
is given a higher priority.  Since Imager's Freetype2 support can also
do font transformations, you may want to give that a higher priority:

  my @old = Imager::Font->priorities(qw(tt ft2 t1));

=head1 AUTHOR

Arnar M. Hrafnkelsson, addi@umich.edu
And a great deal of help from others - see the README for a complete
list.

=head1 BUGS

You need to modify this class to add new font types.

=head1 SEE ALSO

Imager(3), Imager::Font::FreeType2(3), Imager::Font::Type1(3),
Imager::Font::Win32(3), Imager::Font::Truetype(3), Imager::Font::BBox(3)

 http://imager.perl.org/

=cut


