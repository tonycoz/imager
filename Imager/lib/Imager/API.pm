=head1 NAME

Imager::API - Imager's C API - introduction.

=head1 SYNOPSIS

  #include "imext.h"
  #include "imperl.h"

  DEFINE_IMAGER_CALLBACKS;

  MODULE = Your::Module  PACKAGE = Your::Module

  ...

  BOOT:
    PERL_INITIALIZE_IMAGER_CALLBACKS;
  

=head1 DESCRIPTION

The API allows you to access Imager functions at the C level from XS
and from Inline::C.

The intent is to allow users to:

=over

=item *

write C code that does Imager operations the user might do from Perl,
but faster, for example, the Imager::CountColor example.

=item *

write C code that implements an application specific version of some
core Imager object, for example, Imager::SDL.

=item *

write C code that hooks into Imagers existing methods, such as filter
or file format handlers.

=back

See L<Imager::Inline> for information on using Imager's Inline::C
support.

=head1 Types

The API makes the following types visible:

=over

=item *

i_img - used to represent an image

=item *

i_color - used to represent a color with up to 8 bits per sample.

=item *

i_fcolor - used to represent a color with a double per sample.

=item *

i_fill_t - an abstract fill

=back

At this point there is no consolidated font object type, and hence the
font functions are not visible through Imager's API.

=head2 i_img - images

This contains the dimensions of the image (xsize, ysize, channels),
image metadata (ch_mask, bits, type, virtual), potentially image data
(idata) and the a function table, with pointers to functions to
perform various low level image operations.

The only time you should directly write to any value in this type is
if you're implementing your own image type.

=head2 i_color - 8-bit color

Represents an 8-bit per sample color.  This is a union containing
several different structs for access to components of a color:

=over

=item *

gray - single member gray_color.

=item *

rgb - r, g, b members.

=item *

rgba - r, g, b, a members.

=item *

channels - array of channels.

=back

=head2 i_fcolor - floating point color

Similar to i_fcolor except that each component is a double instead of
an unsigned char.

=head2 i_fill_t - fill objects

Abstract type containing pointers called to perform low level fill
operations.

Unless you're defining your own fill objects you should treat this as
an opaque type.

=head1 Create an XS module using the Imager API

=head2 Foo.xs

You'll need the following in your XS source:

=over

=item *

include the Imager external API header, and the perl interface header:

  #include "imext.h"
  #include "imperl.h"

=item *

create the variables used to hold the callback table:

  DEFINE_IMAGER_CALLBACKS;

=item *

initialize the callback table in your BOOT code:

  BOOT:
    PERL_INITIALIZE_IMAGER_CALLBACKS;

=back

=head2 foo.c

In any other source files where you want to access the Imager API,
you'll need to:

=over

=item *

include the Imager external API header:

  #include "imext.h"

=back

=head2 Makefile.PL

If you're creating an XS module that depends on Imager's API your
Makefile.PL will need to do the following:

=over

=item *

C<use Imager::ExtUtils;>

=item *

include Imager's include directory in INC:

  INC => Imager::ExtUtils->includes

=item *

use Imager's typemap:

  TYPEMAPS => [ Imager::ExtUtils->typemap ]

=item *

include Imager 0.48 as a PREREQ_PM:

   PREREQ_PM =>
   {
    Imager => 0.48,
   },

=back

=head1 AUTHOR

Tony Cook <tony@imager.perl.org>

=head1 SEE ALSO

Imager, Imager::ExtUtils, Imager::APIRef, Imager::Inline

=cut
