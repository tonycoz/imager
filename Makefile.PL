#!perl -w
use strict;
use ExtUtils::MakeMaker;
use Cwd;
use Config;
use File::Spec;
use Getopt::Long;
use vars qw(%Recommends);
use ExtUtils::Manifest qw(maniread);
use vars qw(%formats $VERBOSE $INCPATH $LIBPATH $NOLOG $DEBUG_MALLOC $MANUAL $CFLAGS $LFLAGS $DFLAGS);

#
# IM_INCPATH      colon seperated list of paths to extra include paths
# IM_LIBPATH      colon seperated list of paths to extra library paths
#
# IM_VERBOSE      turns on verbose mode for the library finding and such
# IM_MANUAL       to manually select which libraries are used and which not
# IM_ENABLE       to programmatically select which libraries are used
#                 and which are not
# IM_NOLOG        if true logging will not be compiled into the module
# IM_DEBUG_MALLOC if true malloc debbuging will be compiled into the module
#                 do not use IM_DEBUG_MALLOC in production - this slows
#                 everything down by alot
# IM_CFLAGS       Extra flags to pass to the compiler
# IM_LFLAGS       Extra flags to pass to the linker
# IM_DFLAGS       Extra flags to pass to the preprocessor
# IM_SUPPRESS_PROMPT  Suppress the prompt asking about gif support

getenv();     # get environment variables

my $lext=$Config{'so'};   # Get extensions of libraries
my $aext=$Config{'_a'};

my $help; # display help if set
my @enable; # list of drivers to enable
my @disable; # or list of drivers to disable
my @incpaths; # places to look for headers
my @libpaths; # places to look for libraries
my $noprobe; # non-zero to disable newer probes
my $noexif; # non-zero to disable EXIF parsing of JPEGs
GetOptions("help" => \$help,
           "enable=s" => \@enable,
           "disable=s" => \@disable,
           "incpath=s", \@incpaths,
           "libpath=s" => \@libpaths,
           "noprobe" => \$noprobe,
           "verbose|v" => \$VERBOSE,
           "nolog" => \$NOLOG,
	   "noexif" => \$noexif);

if ($VERBOSE) { 
  print "Verbose mode\n"; 
  require Data::Dumper; 
  import Data::Dumper qw(Dumper);
}

if ($help) {
  usage();
}

my @defines;

if ($NOLOG)   { print "Logging not compiled into module\n"; }
else { 
  push @defines, [ IMAGER_LOG => 1, "Logging system" ];
}

if ($DEBUG_MALLOC) {
  push @defines, [ IMAGER_DEBUG_MALLOC => 1, "Use Imager's DEBUG malloc()" ];
  print "Malloc debugging enabled\n";
}

if (@enable && @disable) {
  print STDERR "Only --enable or --disable can be used, not both, try --help\n";
  exit 1;
}

my %definc;
my %deflib;
my @incs; # all the places to look for headers
my @libs; # all the places to look for libraries

init();       # initialize global data
pathcheck();  # Check if directories exist

if (exists $ENV{IM_ENABLE}) {
  my %en = map { $_, 1 } split ' ', $ENV{IM_ENABLE};
  for my $key (keys %formats) {
    delete $formats{$key} unless $en{$key};
  }
}
if (@enable) {
  my %en = map { $_ => 1 } map { split /,/ } @enable;
  for my $key (keys %formats) {
    delete $formats{$key} unless $en{$key};
  }
}
elsif (@disable) {
  delete @formats{map { split /,/ } @disable};
}

# Pick what libraries are used
if ($MANUAL) {
  manual();
} else {
  automatic();
}

# Make sure there isn't a clash between the gif libraries.
gifcheck();

my $lib_cflags = '';
my $F_LIBS = '';
my $F_OBJECT = '';
for my $frmkey (sort { $formats{$a}{order} <=> $formats{$b}{order} } keys %formats) {
  my $frm = $formats{$frmkey};
  push @defines, [ $frm->{def}, 1, "$frmkey available" ];
  $F_LIBS   .= ' '  .$frm->{libfiles};
  $F_OBJECT .= ' '  .$frm->{objfiles};
  if ($frm->{cflags}) {
    $lib_cflags   .= ' '  .$frm->{cflags};
    ++$definc{$_} for map { /^-I(.*)$/ ? ($1) : () }
      grep /^-I./, split ' ', $frm->{cflags};
  }
}

unless ($noexif) {
  print "EXIF support enabled\n";
  push @defines, [ 'IMEXIF_ENABLE', 1, "Enable experimental EXIF support" ];
  $F_OBJECT .= ' imexif.o';
}

my $F_INC  = join ' ', map "-I$_", map / / ? qq{"$_"} : $_, 
  grep !$definc{$_}, @incs;
$F_LIBS = join(' ',map "-L$_", map / / ? qq{"$_"} : $_, 
	       grep !$deflib{$_}++, @libs) . $F_LIBS;

my $OSLIBS = '';
my $OSDEF  = "-DOS_$^O";

if ($^O eq 'hpux')                { $OSLIBS .= ' -ldld'; }
if (defined $Config{'d_dlsymun'}) { $OSDEF  .= ' -DDLSYMUN'; }

my @objs = qw(Imager.o draw.o polygon.o image.o io.o iolayer.o
              log.o gaussian.o conv.o pnm.o raw.o feat.o font.o
              filters.o dynaload.o stackmach.o datatypes.o
              regmach.o trans2.o quant.o error.o convert.o
              map.o tags.o palimg.o maskimg.o img16.o rotate.o
              bmp.o tga.o rgb.o color.o fills.o imgdouble.o limits.o hlines.o
              imext.o);

$Recommends{Imager} =
  { 'Parse::RecDescent' => 0 };

my %opts=(
          'NAME'         => 'Imager',
          'VERSION_FROM' => 'Imager.pm',
          'LIBS'         => "$LFLAGS -lm $OSLIBS $F_LIBS",
          'DEFINE'       => "$OSDEF $CFLAGS",
          'INC'          => "$lib_cflags $DFLAGS $F_INC",
          'OBJECT'       => join(' ', @objs, $F_OBJECT),
          clean          => { FILES=>'testout meta.tmp' },
          PM             => gen_PM(),
         );

if ($ExtUtils::MakeMaker::VERSION > 6.06) {
  $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>, Arnar M. Hrafnkelsson';
  $opts{ABSTRACT} = 'Perl extension for Generating 24 bit Images';
}
if ($ExtUtils::MakeMaker::VERSION > 6.10) {
  $opts{NO_META} = 1;
}

make_imconfig(\@defines);

if ($VERBOSE) { print Dumper(\%opts); }
mkdir('testout',0777); # since we cannot include it in the archive.

make_metafile(\%opts);

WriteMakefile(%opts);

exit;


sub MY::postamble {
    my $self = shift;
    my $perl = $self->{PERLRUN} ? '$(PERLRUN)' : '$(PERL)';
'
dyntest.$(MYEXTLIB) : dynfilt/Makefile
	cd dynfilt && $(MAKE) $(PASTHRU)

lib/Imager/Regops.pm : regmach.h regops.perl
	$(PERL) regops.perl regmach.h lib/Imager/Regops.pm

imconfig.h : Makefile.PL
	$(ECHO) "imconfig.h out-of-date with respect to $?"
	$(PERLRUN) Makefile.PL
	$(ECHO) "==> Your Makefile has been rebuilt - re-run your make command <=="
'.qq!
lib/Imager/APIRef.pm : \$(C_FILES) apidocs.perl
	$perl apidocs.perl lib/Imager/APIRef.pm

!;

}

# manual configuration of helper libraries

sub manual {
  print <<EOF;

      Please answer the following questions about
      which formats are avaliable on your computer

press <return> to continue
EOF

  <STDIN>; # eat one return

  for my $frm(sort { $formats{$b}{order} <=> $formats{$a}{order} } keys %formats) {
  SWX:
    if ($formats{$frm}{docs}) { print "\n",$formats{$frm}{docs},"\n\n"; }
    print "Enable $frm support: ";
    my $gz = <STDIN>;
    chomp($gz);
    if ($gz =~ m/^(y|yes|n|no)/i) {
      $gz=substr(lc($gz),0,1);
      if ($gz eq 'n') {
	delete $formats{$frm};
      }
    } else { goto SWX; }
  }
}


# automatic configuration of helper libraries

sub automatic {
  print "Automatic probing:\n" if $VERBOSE;
  for my $frm (sort { $formats{$a}{order} <=> $formats{$b}{order} } keys %formats) {
    delete $formats{$frm} if !checkformat($frm);	
  }
}


sub gifcheck {
  if ($formats{'gif'} and $formats{'ungif'}) {
    print "ungif and gif can not coexist - removing ungif support\n";
    delete $formats{'ungif'};
  }

 RETR:
  if (($formats{'gif'} or $formats{'ungif'}) && !$ENV{IM_SUPPRESS_PROMPT}) {
    print <<EOFF;

You have libgif or libungif installed.  They are both known to have
bugs.  Imager can crash or display other strange behaviour after
reading or writing gif images.  Some of the gif tests can even fail
since they stress some parts of the buggy code.

libungif 4.1.2 and later is safe.  giflib 4.1.3 needs at least one
patch to have all the bugs fixed, see README for details.

Of course it's possible your operating system distributor has patched
all of these problems and you have nothing to worry about.

Do you want to remove gif support? [Y/n]
EOFF
    my $resp = <STDIN>;
    chomp($resp);
    if ($resp ne "n") {
      delete $formats{'gif'};
      delete $formats{'ungif'};
      return;
    }
  }

  for my $frm (qw(gif ungif)) {
    checkformat($frm) if ($MANUAL and $formats{$frm});
  }

  my @dirs;
  for my $frm (grep $formats{$_}, qw(gif ungif)) {
    push(@dirs, @{$formats{$frm}{incdir}}) if $formats{$frm}{incdir};
  }
  my $minor = 0;
  my $major = 0;
  FILES: for my $dir (@dirs) {
    my $h = "$dir/gif_lib.h";
    open H, "< $h" or next;
    while (<H>) {
      if (/GIF_LIB_VERSION\s+"\s*version\s*(\d+)\.(\d+)/i) {
	$major = $1;
	$minor = $2;
	close H;
	last FILES;
      }
    }
    close H;
  }

  # we need the version in a #ifdefable form

  push @defines, [ IM_GIFMAJOR => $major, "Parsed giflib version" ];
  push @defines, [ IM_GIFMINOR => $minor ];
}


sub gd {
  my($path,$chk)=@_;

#    print "checking path $path\n";
  if ( !opendir(DH,$path) ) {
    warn "Cannot open dir $path: $!\n";
    return;
  }
  my @l=grep { $chk->($_) } readdir(DH);
  #    print @l;
  close(DH);
  return map $path, @l;
}


sub checkformat {
  my $frm=shift;

  print "  checkformat($frm)\n" if $VERBOSE;
  
  my $code = $formats{$frm}{'code'};
  if ($code && !$noprobe) {
    print "    Calling probe function\n" if $VERBOSE;
    return 1 if $code->($formats{$frm}, $frm);
  }

  my $libchk=$formats{$frm}{'libcheck'};
  my $incchk=$formats{$frm}{'inccheck'};

  my @l;
  for my $lp (@libs) {
    push(@l, gd($lp,$libchk));
  }

  my @i;
  for my $ip (@incs) {
    push(@i, $ip) if $incchk->($ip,$frm);
  }

  printf("%10s: includes %s - libraries %s\n",$frm,(@i?'found':'not found'),(@l?'found':'not found'));
  $formats{$frm}{incdir} = \@i;
  $formats{$frm}{libdir} = \@l;
  return scalar(@i && @l);
}


sub pathcheck {
  if ($VERBOSE) {
    print "pathcheck\n";
    print "  Include paths:\n";
    for (@incs) { print $_,"\n"; }
  }
  @incs=grep { -d $_ && -r _ && -x _ or ( print("  $_ doesnt exist or is unaccessible - removed.\n"),0) } @incs;

  if ($VERBOSE) {
    print "\nLibrary paths:\n";
    for (@libs) { print $_,"\n"; }
  }
  @libs=grep { -d $_ && -r _ && -x _ or ( print("  $_ doesnt exist or is unaccessible - removed.\n"),0) } @libs;
  print "\ndone.\n";
}


# Format data initialization

# format definition is:
# defines needed
# default include path
# files needed for include (boolean perl code)
# default lib path
# libs needed
# files needed for link (boolean perl code)
# object files needed for the format


sub init {

  my @definc = qw(/usr/include);
  @definc{@definc}=(1) x @definc;
  @incs=(split(/\Q$Config{path_sep}/, $INCPATH),
        map { split /\Q$Config{path_sep}/} @incpaths );
  if ($Config{locincpth}) {
    push @incs, grep -d, split ' ', $Config{locincpth};
  }
  if ($^O =~ /win32/i && $Config{cc} =~ /\bcl\b/i) {
    push(@incs, split /;/, $ENV{INCLUDE}) if exists $ENV{INCLUDE};
  }
  push @incs, grep -d,
      qw(/sw/include 
         /usr/include/freetype2
         /usr/local/include/freetype2
         /usr/local/include/freetype1/freetype
         /usr/include /usr/local/include /usr/include/freetype
         /usr/local/include/freetype);
  if ($Config{ccflags}) {
    my @hidden = map { /^-I(.*)$/ ? ($1) : () } split ' ', $Config{ccflags};
    push @incs, @hidden;
    @definc{@hidden} = (1) x @hidden;
  }

  @libs= ( split(/\Q$Config{path_sep}/,$LIBPATH),
    map { split /\Q$Config{path_sep}/} @libpaths );
  if ($Config{loclibpth}) {
    push @libs, grep -d, split ' ', $Config{loclibpth};
  }
  
  push @libs, grep -d, qw(/sw/lib),  split(/ /, $Config{'libpth'});
  push @libs, grep -d, split / /, $Config{libspath} if $Config{libspath};
  if ($^O =~ /win32/i && $Config{cc} =~ /\bcl\b/i) {
    push(@libs, split /;/, $ENV{LIB}) if exists $ENV{LIB};
  }
  if ($^O eq 'cygwin') {
    push(@libs, '/usr/lib/w32api') if -d '/usr/lib/w32api';
    push(@incs, '/usr/include/w32api') if -d '/usr/include/w32api';
  }
  if ($Config{ldflags}) {
    # some builds of perl put -Ldir into ldflags without putting it in
    # loclibpth, let's extract them
    my @hidden = grep -d, map { /^-L(.*)$/ ? ($1) : () }
      split ' ', $Config{ldflags};
    push @libs, @hidden;
    # don't mark them as seen - EU::MM will remove any libraries
    # it can't find and it doesn't look for -L in ldflags
    #@deflib{@hidden} = @hidden;
  }

  $formats{'jpeg'}={
		    order=>'21',
		    def=>'HAVE_LIBJPEG',
		    inccheck=>sub { -e catfile($_[0], 'jpeglib.h') },
		    libcheck=>sub { $_[0] eq "libjpeg$aext" or $_ eq "libjpeg.$lext" },
		    libfiles=>'-ljpeg',
		    objfiles=>'jpeg.o',
		    docs=>q{
			    In order to use jpeg with this module you need to have libjpeg
			    installed on your computer}
		   };

  $formats{'tiff'}={
		    order=>'23',
		    def=>'HAVE_LIBTIFF',
		    inccheck=>sub { -e catfile($_[0], 'tiffio.h') },
		    libcheck=>sub { $_[0] eq "libtiff$aext" or $_ eq "libtiff.$lext" },
		    libfiles=>'-ltiff',
		    objfiles=>'tiff.o',
		    docs=>q{
			    In order to use tiff with this module you need to have libtiff
			    installed on your computer}
		   };

  $formats{'png'}={
		   order=>'22',
		   def=>'HAVE_LIBPNG',
		   inccheck=>sub { -e catfile($_[0], 'png.h') },
		   libcheck=>sub { $_[0] eq "libpng$aext" or $_[0] eq "libpng.$lext" },
		   libfiles=>'-lpng -lz',
		   objfiles=>'png.o',
		   docs=>q{
			   Png stands for Portable Network Graphics and is intended as
			   a replacement for gif on the web. It is patent free and
			   is recommended by the w3c, you need libpng to use these formats},
                   code => \&png_probe,
		  };

  $formats{'gif'}={
		   order=>'20',
		   def=>'HAVE_LIBGIF',
		   inccheck=>sub { -e catfile($_[0], 'gif_lib.h') },
		   libcheck=>sub { $_[0] eq "libgif$aext" or $_[0] eq "libgif.$lext" },
		   libfiles=>'-lgif',
		   objfiles=>'gif.o',
		   docs=>q{
			   gif is the de facto standard for webgraphics at the moment,
			   it does have some patent problems. If you have giflib and
			   are not in violation with the unisys patent you should use
			   this instead of the 'ungif' option.  Note that they cannot
			   be in use at the same time}
		  };

  $formats{'ungif'}={
		     order=>'21',
		     def=>'HAVE_LIBGIF',
		     inccheck=>sub { -e catfile($_[0], 'gif_lib.h') },
		     libcheck=>sub { $_[0] eq "libungif$aext" or $_[0] eq "libungif.$lext" },
		     libfiles=>'-lungif',
		     objfiles=>'gif.o',
		     docs=>q{
			     gif is the de facto standard for webgraphics at the moment,
			     it does have some patent problems. If you have libungif and
			     want to create images free from LZW patented compression you
			     should use this option instead of the 'gif' option}
		    };

  $formats{'T1-fonts'}={
			order=>'30',
			def=>'HAVE_LIBT1',
			inccheck=>sub { -e catfile($_[0], 't1lib.h') },
			libcheck=>sub { $_[0] eq "libt1$aext" or $_[0] eq "libt1.$lext" },
			libfiles=>'-lt1',
			objfiles=>'',
			docs=>q{
				postscript t1 fonts are scalable fonts. They can include 
				ligatures and kerning information and generally yield good
				visual quality. We depend on libt1 to rasterize the fonts
				for use in images.}
		       };

  $formats{'TT-fonts'}=
    {
     order=>'31',
     def=>'HAVE_LIBTT',
     inccheck=>sub { -e catfile($_[0], 'freetype.h')
                       && !-e catfile($_[0], 'fterrors.h') },
     libcheck=>sub { $_[0] eq "libttf$aext" or $_[0] eq "libttf.$lext" },
     libfiles=>'-lttf',
     objfiles=>'',
     code => \&freetype1_probe,
     docs=>q{
Truetype fonts are scalable fonts. They can include 
kerning and hinting information and generally yield good
visual quality esp on low resultions. The freetype library is
used to rasterize for us. The only drawback is that there
are alot of badly designed fonts out there.}
		       };
  $formats{'w32'} = {
		     order=>40,
		     def=>'HAVE_WIN32',
		     inccheck=>sub { -e catfile($_[0], 'windows.h') },
		     libcheck=>sub { lc $_[0] eq 'gdi32.lib' 
				       || lc $_[0] eq 'libgdi32.a' },
		     libfiles=>$^O eq 'cygwin' ? '-lgdi32' : '',
		     objfiles=>'win32.o',
		     docs => <<DOCS
Uses the Win32 GDI for rendering text.

This currently only works on under normal Win32 and cygwin.
DOCS
		    };
  $formats{'freetype2'} = {
                           order=>'29',
                           def=>'HAVE_FT2',
                           inccheck=>sub { -e catfile($_[0], 'ft2build.h') },
                           libcheck=>sub { $_[0] eq "libfreetype$aext" or $_[0] eq "libfreetype.$lext" },
                           libfiles=>'-lfreetype',
                           objfiles=>'freetyp2.o',
                           docs=><<DOCS,
Freetype 2 supports both Truetype and Type 1 fonts, both of which are
scalable.  It also supports a variety of other fonts.
DOCS
                           code => \&freetype2_probe,
                          };

  # Make fix indent
  for (keys %formats) { $formats{$_}->{docs} =~ s/^\s+/  /mg; }
}



sub gen {
  my $V = $ENV{$_[0]};
  defined($V) ? $V : "";
}


# Get information from environment variables

sub getenv {

  ($VERBOSE,
   $INCPATH,
   $LIBPATH,
   $NOLOG,
   $DEBUG_MALLOC,
   $MANUAL,
   $CFLAGS,
   $LFLAGS,
   $DFLAGS) = map { gen $_ } qw(IM_VERBOSE
				IM_INCPATH
				IM_LIBPATH
				IM_NOLOG
				IM_DEBUG_MALLOC
				IM_MANUAL
				IM_CFLAGS
				IM_LFLAGS
				IM_DFLAGS);

}

sub make_imconfig {
  my ($defines) = @_;

  open CONFIG, "> imconfig.h"
    or die "Cannot create imconfig.h: $!\n";
  print CONFIG <<EOS;
/* This file is automatically generated by Makefile.PL.
   Don't edit this file, since any changes will be lost */

#ifndef IMAGER_IMCONFIG_H
#define IMAGER_IMCONFIG_H
EOS
  for my $define (@$defines) {
    if ($define->[2]) {
      print CONFIG "\n/*\n  $define->[2]\n*/\n\n";
    }
    print CONFIG "#define $define->[0] $define->[1]\n";
  }
  print CONFIG "\n#endif\n";
  close CONFIG;
}

# use pkg-config to probe for libraries
# works around the junk that pkg-config dumps on FreeBSD
sub _pkg_probe {
  my ($pkg) = @_;

  is_exe('pkg-config') or return;

  my $redir = $^O eq 'MSWin32' ? '' : '2>/dev/null';

  !system("pkg-config $pkg --exists $redir");
}

# probes for freetype1 by scanning @incs for the includes and 
# @libs for the libs.  This is done separately because freetype's headers
# are stored in a freetype or freetype1 directory under PREFIX/include.
#
# we could find the files with the existing mechanism, but it won't set
# -I flags correctly.
#
# This could be extended to freetype2 too, but freetype-config catches
# that
sub freetype1_probe {
  my ($frm, $frmkey) = @_;

  my $found_inc;
 INCS:
  for my $inc (@incs) {
    for my $subdir (qw/freetype freetype1/) {
      my $path = File::Spec->catfile($inc, $subdir, 'freetype.h');
      -e $path or next;
      $path = File::Spec->catfile($inc, $subdir, 'fterrors.h');
      -e $path and next;

      $found_inc = File::Spec->catdir($inc, $subdir);
      last INCS;
    }
  }

  my $found_lib;
 LIBS:
  for my $lib (@libs) {
    my $a_path = File::Spec->catfile($lib, "libttf$aext");
    my $l_path = File::Spec->catfile($lib, "libttf.$lext");
    if (-e $a_path || -e $l_path) {
      $found_lib = $lib;
      last LIBS;
    }
  }

  return unless $found_inc && $found_lib;
  printf("%10s: includes %s - libraries %s\n", $frmkey,
	 ($found_inc ? 'found' : 'not found'), 
	 ($found_lib ? 'found' : 'not found'));

  $frm->{cflags} = "-I$found_inc";
  $frm->{libfiles} = "-lttf";

  return 1;
}

# probes for freetype2 by trying to run freetype-config
sub freetype2_probe {
  my ($frm, $frmkey) = @_;

  is_exe('freetype-config') or return;

  my $cflags = `freetype-config --cflags`
    and !$? or return;
  chomp $cflags;

  my $lflags = `freetype-config --libs`
    and !$? or return;
  chomp $lflags;

  # before 2.1.5 freetype-config --cflags could output
  # the -I options in the wrong order, causing a conflict with
  # freetype1.x installed with the same --prefix
  #
  # can happen iff:
  #  - both -Iprefix/include and -Iprefix/include/freetype2 are in cflags
  #    in that order
  #  - freetype 1.x headers are in prefix/include/freetype
  my @incdirs = map substr($_, 2), grep /^-I/, split ' ', $cflags;
  if (@incdirs == 2 
      && $incdirs[1] eq "$incdirs[0]/freetype2"
      && -e "$incdirs[0]/freetype/freetype.h"
      && -e "$incdirs[0]/freetype/fterrid.h") {
    print "** freetype-config provided -I options out of order, correcting\n"
      if $VERBOSE;
    $cflags = join(' ', grep(!/-I/, split ' ', $cflags),
		   map "-I$_", reverse @incdirs);
  }
  $frm->{cflags} = $cflags;
  $frm->{libfiles} = $lflags;

  printf "%10s: configured via freetype-config\n", $frmkey;

  return 1;
}

# probes for libpng via pkg-config
sub png_probe {
  my ($frm, $frmkey) = @_;

  is_exe('pkg-config') or return;

  my $config;
  for my $check_conf (qw(libpng libpng12 libpng10)) {
    if (_pkg_probe($check_conf)) {
      $config = $check_conf;
      last;
    }
  }
  $config or return;

  my $cflags = `pkg-config $config --cflags`
    and !$? or return;

  my $lflags = `pkg-config $config --libs`
    and !$? or return;

  chomp $cflags;
  chomp $lflags;
  $frm->{cflags} = $cflags;
  $frm->{libfiles} = $lflags;

  printf "%10s: configured via `pkg-config $config ...`\n", $frmkey;

  return 1;
}

sub catfile {
  return File::Spec->catfile(@_);
}

sub is_exe {
  my ($name) = @_;

  for my $dir (File::Spec->path) {
    -x catfile($dir, "$name$Config{_exe}")
      and return 1;
  }

  return;
}

sub usage {
  print STDERR <<EOS;
Usage: $0 [--enable feature1,feature2,...] [other options]
       $0 [--disable feature1,feature2,...]  [other options]
       $0 --help
Possible feature names are:
  png gif ungif jpeg tiff T1-fonts TT-fonts freetype2
Other options:
  --verbose | -v
    Verbose library probing (or set IM_VERBOSE in the environment)
  --nolog
    Disable logging (or set IM_NOLOG in the environment)
  --incpath dir
    Add to the include search path
  --libpath dir
    Add to the library search path
  --noprobe
    Don't use pkg-config or freetype2-config to probe for freetype2 and libpng
EOS
  exit 1;

}

# generate the PM MM argument
# I'd prefer to modify the public version, but there doesn't seem to be 
# a public API to do that
sub gen_PM {
  my %pm;
  my $instbase = '$(INST_LIBDIR)';

  # first the basics, .pm and .pod files
  $pm{"Imager.pm"} = "$instbase/Imager.pm";

  my $mani = maniread();

  for my $filename (keys %$mani) {
    if ($filename =~ m!^lib/! && $filename =~ /\.(pm|pod)$/) {
      (my $work = $filename) =~ s/^lib//;
      $pm{$filename} = $instbase . $work;
    }
  }

  # need the typemap
  $pm{typemap} = $instbase . '/Imager/typemap';

  # and the core headers
  for my $filename (keys %$mani) {
    if ($filename =~ /^\w+\.h$/) {
      $pm{$filename} = $instbase . '/Imager/include/' . $filename;
    }
  }

  # and the generated header
  $pm{"imconfig.h"} = $instbase . '/Imager/include/imconfig.h';

  \%pm;
}

sub make_metafile {
  my ($opts) = @_;

  # extract the version
  my $version = MM->parse_version($opts->{VERSION_FROM})
    or die "Could not extract version number from $opts->{VERSION_FROM}\n";

  my $meta = <<YAML;
--- #YAML:1.0
name: $opts->{NAME}
version: $version
version_from: $opts->{VERSION_FROM}
author: $opts->{AUTHOR}
abstract: $opts->{ABSTRACT}
installdirs: site
YAML
  if (keys %{$Recommends{$opts->{NAME}}}) {
    $meta .= "recommends:\n";
    while (my ($module, $version) = each %{$Recommends{$opts->{NAME}}}) {
      $meta .= "  $module: $version\n";
    }
  }
  $meta .= <<YAML;
license: perl
dynamic_config: 1
distribution_type: module
generated_by: $opts->{NAME} version $version
YAML
  my $save_meta;
  if (open META, "< META.yml") {
    my $old_meta = do { local $/; <META> };
    close META;

    $save_meta = $old_meta ne $meta;
  }
  else {
    ++$save_meta;
  }
  if ($save_meta) {
    print "Updating META.yml\n";
    open META, "> META.yml" or die "Cannot create META.yml: $!";
    print META $meta;
    close META;
  }
}
