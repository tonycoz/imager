#!perl -w
use ExtUtils::MakeMaker;
use Cwd;
use Config;
use File::Spec;
use Getopt::Long;

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

my $help;
my @enable;
my @disable;
my @incpaths;
my @libpaths;
my $noprobe;
GetOptions("help" => \$help,
           "enable=s" => \@enable,
           "disable=s" => \@disable,
           "incpath=s", \@incpaths,
           "libpath=s" => \@libpaths,
           "noprobe" => \$noprobe);

if ($help) {
  usage();
}

if (@enable && @disable) {
  print STDERR "Only --enable or --disable can be used, not both, try --help\n";
  exit 1;
}

getenv();     # get environment variables
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

my @defines;

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
for my $frmkey (keys %formats) {
  my $frm = $formats{$frmkey};
  push @defines, [ $frm->{def}, 1, "$frmkey available" ];
  $F_LIBS   .= ' '  .$frm->{libfiles};
  $F_OBJECT .= ' '  .$frm->{objfiles};
  $lib_cflags   .= ' '  .$frm->{cflags} if $frm->{cflags};
}

$F_INC  = join ' ', map "-I$_", map / / ? qq{"$_"} : $_, 
  grep !exists $definc{$_}, @incs;
$F_LIBS = join(' ',map "-L$_", map / / ? qq{"$_"} : $_, @libs) . $F_LIBS;

$OSLIBS = '';
$OSDEF  = "-DOS_$^O";

if ($^O eq 'hpux')                { $OSLIBS .= ' -ldld'; }
if (defined $Config{'d_dlsymun'}) { $OSDEF  .= ' -DDLSYMUN'; }

@objs = qw(Imager.o draw.o polygon.o image.o io.o iolayer.o
           log.o gaussian.o conv.o pnm.o raw.o feat.o font.o
	   filters.o dynaload.o stackmach.o datatypes.o
	   regmach.o trans2.o quant.o error.o convert.o
	   map.o tags.o palimg.o maskimg.o img16.o rotate.o
           bmp.o tga.o rgb.o color.o fills.o imgdouble.o);

%opts=(
       'NAME'         => 'Imager',
       'VERSION_FROM' => 'Imager.pm',
       'LIBS'         => "$LFLAGS -lm $OSLIBS $F_LIBS",
       'DEFINE'       => "$OSDEF $CFLAGS",
       'INC'          => "$lib_cflags $DFLAGS $F_INC",
       'OBJECT'       => join(' ', @objs, $F_OBJECT),
       clean          => { FILES=>'testout meta.tmp' },
      );

if ($ExtUtils::MakeMaker::VERSION > 6.06) {
  $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>, Arnar M. Hrafnkelsson';
  $opts{ABSTRACT} = 'Perl extension for Generating 24 bit Images';
}

make_imconfig(\@defines);

if ($VERBOSE) { print Dumper(\%opts); }
mkdir('testout',0777); # since we cannot include it in the archive.
WriteMakefile(%opts);

exit;


sub MY::postamble {
'
dyntest.$(MYEXTLIB) : dynfilt/Makefile
	cd dynfilt && $(MAKE) $(PASTHRU)

lib/Imager/Regops.pm : regmach.h regops.perl
	$(PERL) regops.perl regmach.h lib/Imager/Regops.pm

imconfig.h: Makefile.PL
	$(ECHO) "imconfig.h out-of-date with respect to $?"
	$(PERLRUN) Makefile.PL
	$(ECHO) "==> Your Makefile has been rebuilt - re-run your make command <=="
';
}

sub MY::metafile {
  my ($self) = @_;

  my $meta = <<YAML;
--- #YAML:1.0
name: Imager
version: $self->{VERSION}
version_from: $self->{VERSION_FROM}
author: $self->{AUTHOR}
abstract: $self->{ABSTRACT}
installdirs: $self->{INSTALLDIRS}
recommends:
  Parse::RecDescent: 0
license: perl
dynamic_config: 1
distribution_type: module
generated_by: Imager version $self->{VERSION}
YAML
  open META, "> meta.tmp" or die "Cannot create meta.tmp: $!";
  print META $meta;
  close META;
  
  return sprintf "metafile :\n\t\$(CP) meta.tmp META.yml\n";
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
    $gz = <STDIN>;
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
  for $frm(keys %formats) {
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

  push @defines, [ IM_GIFMAJOR, $major, "Parsed giflib version" ];
  push @defines, [ IM_GIFMINOR, $minor ];
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
  
  my $code = $formats{$frm}{'code'};
  if ($code && !$noprobe) {
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
    for (@incs) { print $_,"\n"; }
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

  @definc{'/usr/include'}=();
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

  @libs= ( split(/\Q$Config{path_sep}/,$LIBPATH),
    map { split /\Q$Config{path_sep}/} @libpaths );
  if ($Config{loclibpth}) {
    push @libs, grep -d, split ' ', $Config{loclibpth};
  }
  push @libs, grep -d, qw(/sw/lib),  split(/ /, $Config{'libpth'});
  if ($^O =~ /win32/i && $Config{cc} =~ /\bcl\b/i) {
    push(@libs, split /;/, $ENV{LIB}) if exists $ENV{LIB};
  }
  if ($^O eq 'cygwin') {
    push(@libs, '/usr/lib/w32api') if -d '/usr/lib/w32api';
    push(@incs, '/usr/include/w32api') if -d '/usr/lib/w32api';
  }

  my $lext=$Config{'so'};   # Get extensions of libraries
  my $aext=$Config{'_a'};

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

  if ($VERBOSE) { print "Verbose mode\n"; require Data::Dumper; import Data::Dumper qw(Dumper);}

  if ($NOLOG)   { print "Logging not compiled into module\n"; }
  else { 
    push @defines, [ IMAGER_LOG => 1, "Logging system" ];
  }

  if ($DEBUG_MALLOC) {
    push @defines, [ IMAGER_DEBUG_MALLOC => 1, "Use Imager's DEBUG malloc()" ];
    print "Malloc debugging enabled\n";
  }

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

# probes for freetype2 by trying to run freetype-config
sub freetype2_probe {
  my ($frm, $frmkey) = @_;

  is_exe('freetype-config') or return;

  my $cflags = `freetype-config --cflags`
    and !$? or return;
  chomp $cflags;
  
  $frm->{cflags} = $cflags;
  my $lflags = `freetype-config --libs`
    and !$? or return;
  chomp $lflags;
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
Usage: $0 [--enable feature1,feature2,...] [--incpath ...] [--libpath ...]
       $0 [--disable feature1,feature2,...] [--incpath ...] [--libpath ...]
       $0 --help
Possible feature names are:
  png gif ungif jpeg tiff T1-fonts TT-fonts freetype2
EOS
  exit 1;

}
