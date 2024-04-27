#!perl -w
use strict;
use ExtUtils::MakeMaker;
use Cwd;
use Config;
use File::Spec;
use Getopt::Long;
use ExtUtils::Manifest qw(maniread);
use ExtUtils::Liblist;
use vars qw(%formats $VERBOSE $INCPATH $LIBPATH $NOLOG $DEBUG_MALLOC $MANUAL $CFLAGS $LFLAGS $DFLAGS);
use lib 'inc', 'lib';
use Imager::Probe;

# EU::MM runs Makefile.PL all in the same process, so sub-modules will
# see this
our $BUILDING_IMAGER = 1;

# used to display a summary after we've probed the world
our %IMAGER_LIBS;

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

my $KEEP_FILES = $ENV{IMAGER_KEEP_FILES};

# make sure --verbose will dump environment settings
if (grep $_ =~ /^--?v(?:erbose)?$/, @ARGV) {
  $VERBOSE = 1;
}

# modules/features bundled with Imager that can be enabled/disabled
# withs --enable/--disable
my @bundled = qw(FT1 FT2 GIF JPEG PNG T1 TIFF W32);

# extra modules bundled with Imager not available on CPAN
my @extras = qw(CountColor DynTest ICO SGI Mandelbrot Flines);

# alternate names for modules
my %bundled_names = qw(win32 w32 tt ft1);

getenv();     # get environment variables

my $lext=$Config{'so'};   # Get extensions of libraries
my $aext=$Config{'_a'};

my $help; # display help if set
my @enable; # list of drivers to enable
my @disable; # or list of drivers to disable
my @incpaths; # places to look for headers
my @libpaths; # places to look for libraries
my $coverage; # build for coverage testing
my $assert; # build with assertions
my $trace_context; # trace context management to stderr
my $uselcms; # use lcms instead of the built-in tone curve only CMS.
my @Accflags;
my @Aldflags;
my @Alddlflags;
my @Aoptimize;

GetOptions("help" => \&usage,
           "enable=s" => \@enable,
           "disable=s" => \@disable,
           "incpath=s", \@incpaths,
           "libpath=s" => \@libpaths,
           "verbose|v" => \$VERBOSE,
           "nolog" => \$NOLOG,
	   'coverage' => \$coverage,
	   "assert|a" => \$assert,
	   "tracecontext" => \$trace_context,
	   "uselcms" => \$uselcms,
           "Accflags=s" => \@Accflags,
           "Aldflags=s" => \@Aldflags,
           "Alddlflags=s" => \@Alddlflags,
           "Aoptimize=s" => \@Aoptimize,
	  )
  or die "Error in command-line arguments, try $^X $0 --help\n";

if ($ENV{AUTOMATED_TESTING}) {
  $assert = 1;
  $VERBOSE = 1;
}

if ($coverage) {
    if ($Config{gccversion}) {
        push @Aoptimize, '-ftest-coverage -fprofile-arcs';
        push @Alddlflags, '-ftest-coverage -fprofile-arcs';
    }
    else {
	die "Don't know the coverage C flags for your compiler\n";
    }
}

my %CmdConfig =
  (
    ccflags   => \@Accflags,
    ldflags   => \@Aldflags,
    lddlflags => \@Alddlflags,
    optimize  => \@Aoptimize,
   );
my %MyConfig;
for my $arg (@ARGV) {
  if ($arg =~ /^(\w+)=(.*)$/) {
    $MyConfig{lc $1} = $2;

    if ($CmdConfig{$1} && @{$CmdConfig{$1}}) {
      die "--A\L$1\E option overridden by $1=$2 on command-line\n";
    }
  }
}

for my $name (qw(ccflags ldflags lddlflags optimize cc)) {
  unless (exists $MyConfig{$name}) {
    $MyConfig{$name} = $Config{$name};
    if ($CmdConfig{$name} && @{$CmdConfig{$name}}) {
      $MyConfig{$name} .= " @{$CmdConfig{$name}}";
    }
  }
}

probe_c99_flag();

setenv();

if ($VERBOSE) { 
  print "Verbose mode\n"; 
  require Data::Dumper; 
  Data::Dumper->import("Dumper");
}

my @defines;

if ($NOLOG)   { print "Logging not compiled into module\n"; }
else { 
  push @defines, [ IMAGER_LOG => 1, "Logging system" ];
}

if ($assert) {
  push @defines, [ IM_ASSERT => 1, "im_assert() are effective" ];
}

if ($DEBUG_MALLOC) {
  push @defines, [ IMAGER_DEBUG_MALLOC => 1, "Use Imager's DEBUG malloc()" ];
  print "Malloc debugging enabled\n";
}

if (@enable && @disable) {
  print STDERR "Only --enable or --disable can be used, not both, try --help\n";
  exit 1;
}
if (!@enable && !@disable) {
  @disable = qw(T1 FT1);
}

my %definc;
my %deflib;
my @incs; # all the places to look for headers
my @libs; # all the places to look for libraries

init();       # initialize global data
pathcheck();  # Check if directories exist

my @enabled_bundled;
if (exists $ENV{IM_ENABLE}) {
  push @enable, split ' ', $ENV{IM_ENABLE};
}
if (@enable) {
  my %en = map { lc $_ => 1 } map_bundled(@enable);
  @enabled_bundled = grep $en{lc $_}, @bundled;
}
elsif (@disable) {
  my %dis = map { lc $_ => 1 } map_bundled(@disable);
  @enabled_bundled = grep !$dis{lc $_}, @bundled;
}
else {
  @enabled_bundled = @bundled;
}

# Pick what libraries are used
if ($MANUAL) {
  manual();
} else {
  automatic();
}

my @objs = qw(Imager.o context.o draw.o polygon.o image.o io.o iolayer.o
              log.o gaussian.o conv.o pnm.o raw.o feat.o combine.o
              filters.o dynaload.o stackmach.o datatypes.o
              regmach.o trans2.o quant.o error.o convert.o
              map.o tags.o palimg.o maskimg.o img8.o img16.o rotate.o
              bmp.o tga.o color.o fills.o imgdouble.o limits.o hlines.o
              imext.o scale.o rubthru.o render.o paste.o compose.o flip.o
	      perlio.o imexif.o trim.o data.o imgflt.o linimg16.o linimgdbl.o);

push @objs, $uselcms ? "imcms.o" : "imnocms.o";

my $lib_define = '';
my $lib_inc = '';
my $lib_libs = '';
for my $frmkey (sort { $formats{$a}{order} <=> $formats{$b}{order} } keys %formats) {
  my $frm = $formats{$frmkey};
  if ($frm->{enabled}) {
    push @defines, [ $frm->{def}, 1, "$frmkey available" ];
    push @objs, $frm->{objfiles};
    $lib_define .= " $frm->{DEFINE}" if $frm->{DEFINE};
    $lib_inc    .= " $frm->{INC}"    if $frm->{INC};
    $lib_libs   .= " $frm->{LIBS}"   if $frm->{LIBS};
  }
}

my $OSLIBS = '';
my $OSDEF  = "-DOS_$^O";

if ($^O eq 'hpux')                { $OSLIBS .= ' -ldld'; }
if (defined $Config{'d_dlsymun'}) { $OSDEF  .= ' -DDLSYMUN'; }

if ($Config{useithreads}) {
  if ($Config{i_pthread}) {
    print "POSIX threads\n";
    push @objs, "mutexpthr.o";
  }
  elsif ($^O eq 'MSWin32') {
    print "Win32 threads\n";
    push @objs, "mutexwin.o";
  }
  else {
    print "Unsupported threading model\n";
    push @objs, "mutexnull.o";
    if ($ENV{AUTOMATED_TESTING}) {
      die "OS unsupported: no threading support code for this platform\n";
    }
  }
}
else {
  print "No threads\n";
  push @objs, "mutexnull.o";
}

my @typemaps = qw(typemap.local typemap);
if ($] < 5.008) {
    unshift @typemaps, "typemap.oldperl";
}

if ($trace_context) {
  $CFLAGS .= " -DIMAGER_TRACE_CONTEXT";
}

my $tests = 't/*.t t/*/*.t';
if (-d "xt" && scalar(() = glob("xt/*.t"))) {
  $tests .= " xt/*.t";
}

my %opts=
  (
   NAME         => 'Imager',
   VERSION_FROM => 'Imager.pm',
   LIBS         => "$LFLAGS -lm $lib_libs $OSLIBS",
   DEFINE       => "$OSDEF $lib_define $CFLAGS",
   INC          => "$lib_inc $DFLAGS",
   OBJECT       => join(' ', @objs),
   DIR          => [ sort grep -d, @enabled_bundled, @extras ],
   clean          => { FILES=>'testout rubthru.c scale.c conv.c  filters.c gaussian.c render.c rubthru.c *.gcda cov-int' },
   PM             => gen_PM(),
   PREREQ_PM      =>
   { 
    'Test::More' => 0.99,
    'Scalar::Util' => 1.00,
    'XSLoader'    => 0,
    'Exporter'    => '5.57',
   },
   TYPEMAPS       => \@typemaps,
   test           => { TESTS => $tests },
   depend         => make_depend(),
   CCFLAGS        => $MyConfig{ccflags},
   LDDLFLAGS      => $MyConfig{lddlflags},
   OPTIMIZE       => $MyConfig{optimize},
  );

if (eval { ExtUtils::MakeMaker->VERSION('6.06'); 1 }) {
  $opts{AUTHOR} = 'Tony Cook <tonyc@cpan.org>, Arnar M. Hrafnkelsson';
  $opts{ABSTRACT} = 'Perl extension for Generating 24 bit Images';
}

if (eval { ExtUtils::MakeMaker->VERSION('6.46'); 1 }) {
  $opts{LICENSE} = "perl_5";
  $opts{META_MERGE} =
    {
     'meta-spec' =>
     {
      version => "2",
      url => "https://metacpan.org/pod/CPAN::Meta::Spec",
     },
     prereqs =>
     {
      runtime =>
      {
       recommends =>
       {
        "Parse::RecDescent" => 0
       },
       requires =>
       {
	'Scalar::Util' => "1.00",
	XSLoader => 0,
	'Exporter'    => '5.57',
       },
      },
      build =>
      {
       requires =>
       {
	XSLoader => 0,
	'Exporter' => '5.57',
       },
      },
      test =>
      {
       requires =>
       {
	'Test::More' => "0.99",
       },
      },
     },
     dynamic_config => 0,
     no_index =>
     {
      directory =>
      [
       "PNG",
       "GIF",
       "TIFF",
       "JPEG",
       "W32",
       "FT2",
       "T1",
      ],
     },
     resources =>
     {
      homepage => "http://imager.perl.org/",
      repository =>
      {
       url => "git://github.com/tonycoz/imager.git",
       web => "https://github.com/tonycoz/imager.git",
       type => "git",
      },
      bugtracker =>
      {
       web => "https://github.com/tonycoz/imager/issues",
      },
     },
    };
}

make_imconfig(\@defines);

if ($VERBOSE) { print Dumper(\%opts); }
mkdir('testout',0777); # since we cannot include it in the archive.

-d "probe" and rmdir "probe";

WriteMakefile(%opts);

my @good;
my @bad;
for my $name (sort { lc $a cmp lc $b } keys %IMAGER_LIBS) {
  if ($IMAGER_LIBS{$name}) {
    push @good, $name;
  }
  else {
    push @bad, $name;
  }
}

print "\n";
print "Libraries found:\n" if @good;
print "  $_\n" for @good;
print "Libraries *not* found:\n" if @bad;
print "  $_\n" for @bad;

exit;


sub MY::postamble {
    my $self = shift;
    my $perl = $self->{PERLRUN} ? '$(PERLRUN)' : '$(PERL)';
    my $mani = maniread;

    my @ims = grep /\.im$/, keys %$mani;
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
lib/Imager/APIRef.pod : \$(C_FILES) \$(H_FILES) apidocs.perl
	$perl apidocs.perl lib/Imager/APIRef.pod

!.join('', map _im_rule($perl, $_), @ims)

}

# we provide finer dependencies
{
  package MY;
  sub MY::top_targets {
    my $self = shift;

    my @base = split /\n/, $self->SUPER::top_targets;

    @base = grep !/^\$\(O_FILES\)\s+:/, @base;

    return join "\n", @base;
  }
}

sub _im_rule {
  my ($perl, $im) = @_;

  (my $c = $im) =~ s/\.im$/.c/;
  return <<MAKE;

$c: $im lib/Imager/Preprocess.pm
	$perl -Ilib -MImager::Preprocess -epreprocess $im $c

MAKE

}

# manual configuration of helper libraries

sub manual {
  print <<EOF;

      Please answer the following questions about
      which formats are avaliable on your computer

      Warning: if you use manual configuration you are responsible for
      configuring extra include/library directories as necessary using
      INC and LIBS command-line assignments.

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
      if ($gz =~ /y/i) {
	$formats{$frm}{enabled} = 1;
	$IMAGER_LIBS{$frm} = 1;
      }
    } else { goto SWX; }
  }
}


# automatic configuration of helper libraries

sub automatic {
  print "Automatic probing:\n" if $VERBOSE;

  if (grep $_ eq "FT1", @enabled_bundled) {
    my %probe =
      (
       name => "FT1",
       inccheck => sub { -e File::Spec->catfile($_[0], "ftnameid.h") },
       libbase => "ttf",
       testcode => _ft1_test_code(),
       testcodeheaders => [ "freetype.h", "stdio.h" ],
       incpaths => \@incpaths,
       libpaths => \@libpaths,
       alternatives =>
       [
	{
	 incsuffix => "freetype",
	}
       ],
       verbose => $VERBOSE,
      );
    my $probe_res = Imager::Probe->probe(\%probe);
    $IMAGER_LIBS{FT1} = defined $probe_res;
    if ($probe_res) {
      $formats{FT1}{enabled} = 1;
      @{$formats{FT1}}{qw/DEFINE INC LIBS/} =
	@$probe_res{qw/DEFINE INC LIBS/};
    }
  }
  if ($uselcms) {
    my %probe =
      (
       name => "LCMS2",
       inccheck => sub { -e File::Spec->catfile($_[0], "lcms2.h") },
       libbase => "lcms2",
       incpaths => \@incpaths,
       libpaths => \@libpaths,
       verbose => $VERBOSE,
       testcodeheaders => [ "stdio.h", "lcms2.h" ],
       testcode => <<'CODE'
  if (LCMS_VERSION < 2060) {
    fprintf(stderr, "LCMS: LCMS 2.6 or later required\n");
    return 1;
  }
  return 0;
CODE
      );
    my $probe_res = Imager::Probe->probe(\%probe);
    $probe_res
      or die "OS unsupported: LCMS2 2.6 required";
    $formats{LCMS2}{enabled} = 1;
    @{$formats{LCMS2}}{qw/DEFINE INC LIBS/} =
      @$probe_res{qw/DEFINE INC LIBS/};
  }
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
  @incs=
    (
     split(/\Q$Config{path_sep}/, $INCPATH),
     map _tilde_expand($_), map { split /\Q$Config{path_sep}/ } @incpaths 
    );
  if ($Config{locincpth}) {
    push @incs, grep -d, split ' ', $Config{locincpth};
  }
  if ($^O =~ /win32/i && $Config{cc} =~ /\bcl\b/i) {
    push(@incs, split /;/, $ENV{INCLUDE}) if exists $ENV{INCLUDE};
  }
  if ($Config{incpath}) {
    push @incs, grep -d, split /\Q$Config{path_sep}/, $Config{incpath};
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
    map _tilde_expand($_), map { split /\Q$Config{path_sep}/} @libpaths );
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
  push @libs, grep -d, qw(/usr/local/lib);

  $formats{FT1}=
    {
     order=>'31',
     def=>'HAVE_LIBTT',
     objfiles=>'fontft1.o',
     LIBS => "-lttf",
     docs=>q{
Freetype 1.x supports Truetype fonts and is obsoleted by Freetype 2.x.

It's probably insecure.
}
		       };
  $formats{LCMS2} =
    {
     order => 32,
     def => "HAVE_LCMS2",
     objfiles => '',
     docs => "LCMS2.6 or higher",
    };
  # Make fix indent
  for (keys %formats) { $formats{$_}->{docs} =~ s/^\s+/  /mg; }
}



sub gen {
  my $V = $ENV{$_[0]};
  print "  $_[0]: '$V'\n"
      if $VERBOSE && defined $V;
  defined($V) ? $V : "";
}


# Get information from environment variables

sub getenv {

  $VERBOSE ||= gen("IM_VERBOSE");

  print "Environment config:\n" if $VERBOSE;

  ($INCPATH,
   $LIBPATH,
   $NOLOG,
   $DEBUG_MALLOC,
   $MANUAL,
   $CFLAGS,
   $LFLAGS,
   $DFLAGS) = map { gen $_ } qw(IM_INCPATH
				IM_LIBPATH
				IM_NOLOG
				IM_DEBUG_MALLOC
				IM_MANUAL
				IM_CFLAGS
				IM_LFLAGS
				IM_DFLAGS);
}

# populate the environment so that sub-modules get the same info
sub setenv {
  $ENV{IM_VERBOSE} = 1 if $VERBOSE;
  $ENV{IM_INCPATH} = join $Config{path_sep}, @incpaths if @incpaths;
  $ENV{IM_LIBPATH} = join $Config{path_sep}, @libpaths if @libpaths;
}

sub make_imconfig {
  my ($defines) = @_;

  open my $config, "> imconfig.h"
    or die "Cannot create imconfig.h: $!\n";
  print $config <<EOS;
/* This file is automatically generated by Makefile.PL.
   Don't edit this file, since any changes will be lost */

#ifndef IMAGER_IMCONFIG_H
#define IMAGER_IMCONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

EOS
  for my $define (@$defines) {
    if ($define->[2]) {
      print $config "\n/*\n  $define->[2]\n*/\n\n";
    }
    print $config "#define $define->[0] $define->[1]\n";
  }
  if ($Config{gccversion} && $Config{gccversion} =~ /^([0-9]+)/ && $1 > 3) {
    print $config <<EOS;
/*

Compiler supports the GCC __attribute__((format...)) syntax.

*/

#define IMAGER_FORMAT_ATTR 1

EOS
  }

  if ($Config{d_snprintf}) {
    print $config <<EOS;
/* We can use snprintf() */
#define IMAGER_SNPRINTF 1

EOS
  }

  if ($Config{d_vsnprintf}) {
    print $config <<EOS;
/* We can use vsnprintf() */
#define IMAGER_VSNPRINTF 1

EOS
  }

  print $config <<EOS;
/*
 Type and format code for formatted output as with printf.

 This is intended for formatting i_img_dim values.
*/
typedef $Config{ivtype} i_dim_format_t;
#define i_DF $Config{ivdformat}

EOS

  if ($Config{d_static_inline}) {
    print $config "#define IMAGER_STATIC_INLINE $Config{perl_static_inline}\n";
  }
  else {
    print $config "#define IMAGER_STATIC_INLINE static\n";
  }
  
  print $config <<'EOS';

/*

Define either IMAGER_LITTLE_ENDIAN or IMAGER_BIG_ENDIAN depending on
the byte order of the platform.  Mixed endian is not supported.

*/

EOS
  my $byteorder = $Config{byteorder};
  if ($byteorder eq '1234' || $byteorder eq '12345678') {
    print $config <<'EOS'
#define IMAGER_LITTLE_ENDIAN
EOS
  }
  elsif ($byteorder eq '4321' || $byteorder eq '87654321') {
    print $config <<'EOS'
#define IMAGER_BIG_ENDIAN
EOS
  }
  else {
    # no support for mixed byte order
    die "Unknown byte order $byteorder";
  }
  if ($Config{d_mmap} && $Config{d_munmap}) {
    print $config <<'EOS'
/* we can use mmap()/munmap() */
#define IMAGER_POSIX_MMAP

EOS
  }
  my ($u16type, $need_stdint_uint16) = probe_uint16();
  unless ($u16type) {
    die <<'EOS'
No unsigned 16-bit integer type found.

If your system defines an unsigned 16-bit type, please send me the
config.sh generated for your system and the name of the type.

EOS
  }
  my ($ptr_int_type, $need_stdint_ptr_int) = probe_ptr_int_type();
  my $need_stdint = "$need_stdint_uint16$need_stdint_ptr_int" =~ /define/
    ? "define" : "undef";

  print $config <<EOS;
#$need_stdint IMAGER_USE_STDINT_H
#ifdef IMAGER_USE_STDINT_H
#  include <stdint.h>
#endif

/* Used for 16-bit images, and the type of the data returned by
  i_img_data() for i_16_bit.
*/

typedef $u16type i_sample16_t;

/*
An unsigned integer type that is the same size or larger than a void*

C99 optionally allows an implementation to define uintptr_t, but it
isn't required.
*/

typedef $ptr_int_type im_ptr_int;
EOS

  my $f16type = probe_float16();
  my $f16def = $f16type ? 'define' : 'undef';
    print $config <<EOS;
/*
Do we have 16-bit floating point.

If we do, add a i_fsample16_t typedef for it.

To make the type and macro visible you need to define
IMAGER_WANT_FLOAT16 before including imconfig.h.  This lets you build
Imager with compiler options needed to enable 16-bit floating point
without breaking modules that use Imager's API that don't need 16-bit
floats, but are built with the standard perl C compiler flags.
*/

#ifdef IMAGER_WANT_FLOAT16
#$f16def IMAGER_HAVE_FLOAT16
EOS
  if ($f16type) {
    print $config <<EOS;
typedef $f16type i_fsample16_t;
EOS
  }
  print $config <<"EOS";
#endif

/* size of pointers, for use in preprocessing */
#define IMAGER_PTR_SIZE $Config{ptrsize}
EOS

  my ($type, $defs) = probe_aligned_alloc();
  print $config <<EOS;

/*
 IM_ALIGNED_ALLOC_TYPE selects one of three possible aligned
 allocation functions.

  They all have different parameter lists, so we can't just define a
  symbol that names the function.

  MSVC doesn't implement the C11 function
*/

$defs
#define IM_ALIGNED_ALLOC_TYPE $type
EOS

  my $nr = probe_noreturn();
  print $config <<EOS;
/*

IMAGER_NORETURN is a function attribute that can be used before a
function definition (and declaration) to indicate that the function
doe not return.

Similar to perl.h's __attribute__noreturn__ except this is expected to
be used *before* the declaration, and doesn't invade the
implementations reserved namespace.

C++ compatible definitions are provided if possible.

*/

#ifdef __cplusplus
#  ifdef __has_cpp_attribute
#    define IMAGER_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#  else
#    define IMAGER_HAS_CPP_ATTRIBUTE(x) 0
#  endif
#  if IMAGER_HAS_CPP_ATTRIBUTE(noreturn)
#    define IMAGER_NORETURN [[noreturn]]
#  else
#    define IMAGER_NORETURN
#  endif
#else
#  define IMAGER_NORETURN $nr
#endif
EOS

  print $config <<'EOS';

/*

IMAGER_ALIGN_SIZE is intended for use in helping to generate
alignment values suitable for building with automatic vectorization.

For now this is a fixed multiplier.  This may be modified in the
future to be more adaptive to the architecture and the size.

*/

#define IMAGER_ALIGN_SIZE(type) (8 * sizeof(type))

EOS

  print $config <<'EOS';

#ifdef __cplusplus
}
#endif

#endif
EOS
  close $config;
}

sub probe_uint16 {
  local $| = 1;
  print "Looking for a 16-bit unsigned integer type ... ";
  for my $case ([ "unsigned short", "undef" ], [ "uint16_t", "define" ]) {
    my ($type, $stdint) = @$case;
    my $verb = $VERBOSE ? "define" : "undef";
    my $code = <<EOS;
#$stdint I_STDINT
#ifdef I_STDINT
#include <stdint.h>
#endif
#$verb VERBOSE
#ifdef VERBOSE
#include <stdio.h>
#define DUMP(x) x
#else
#define DUMP(x) ((void)0)
#endif

int main(int argc, char **argv) {
  $type i = 0xFFFF;
  if (sizeof(i) != 2) {
    DUMP( printf("sizeof($type) is not 2\\n") );
    return 1;
  }
  if (($type)(i+1U) != 0) {
    DUMP( printf("$type arithmetic test failed\\n") );
    return 1;
  }
  return 0;
}
EOS
    print $code if $VERBOSE;
    if (try_compile_and_link($code, run => 1)) {
      print "found $type\n";
      return ( $type, $stdint );
    }
  }
}

# try to find a float16 type
# returns the type name
sub probe_float16 {
  # in order of preference
  # we only use this type as a storage type
  local $| = 1;
  print "Looking for a 16-bit float type ... ";
  for my $type (qw(_Float16 __fp16)) {
    if (try_compile_and_link(<<EOS)) {
#define __STDC_WANT_IEC_60559_TYPES_EXT__
#include <float.h>
int main(int argc, char **argv) {
  float a = 1.0;
  $type b = a;
  return 0;
}
EOS
      print "found $type\n";
      return $type;
    }
  }
  print "no 16-bit float type found\n";
  return;
}

# try to find an unsigned integer type that can hold a void *
sub probe_ptr_int_type {
  my $code = <<'EOS';
#include <stdint.h>

int x = 0;

int main() {
  uintptr_t y = (uintptr_t)(void*)&x;
  int *z = (int *)(void *)y;

  return *z;
}
EOS
  if (try_compile_and_link($code)) {
    return ("uintptr_t", "define");
  }

  for my $type ("unsigned", "unsigned long", "unsigned long long") {
    my $code = <<"EOS";
int x = 0;

int main() {
  $type y = ($type)(void*)&x;
  int *z = (int *)(void *)y;

  return *z;
}
EOS
    if (try_compile_and_link($code)) {
      return ($type, "undef");
    }
  }

  die "Cannot find an unsigned integer type that can hold a pointer\n";
}

# probe for an allocation function that we can specify alignment for.
# these functions have different parameter handling, so we can't just
# define names here
sub probe_aligned_alloc {
  my @types = qw(IM_ALIGNED_ALLOC_C11 IM_ALIGNED_ALLOC_POSIX IM_ALIGNED_ALLOC_MSVC);
  my %types;
  @types{@types} = 0 .. $#types;
  my @keys = sort { $types{$a} <=> $types{$b} } keys %types;
  my @deftypes = ( @types, "IM_ALIGNED_ALLOC_NONE" );
  my $defs = join("", map { "#define $deftypes[$_] $_\n" } 0..$#deftypes );

  if (my $type = $ENV{IM_ALIGNED_ALLOC}) {
    grep "IM_ALIGNED_ALLOC_$type" eq $_, @types
      or do {
        my @etypes = @types;
        s/^IM_ALIGNED_ALLOC_// for @etypes;
        die "IM_ALIGNED_ALLOC environment variable set but invalid, must be one of @etypes\n";
      };
    return ("IM_ALIGNED_ALLOC_$type", $defs)
  }

  for my $type (@keys) {
    my $code = <<EOS;
#include <stdlib.h>
#include <stdio.h>

$defs

#define ALLOC_TYPE $type

#if ALLOC_TYPE == IM_ALIGNED_ALLOC_MSVC
#include <malloc.h>
#endif

void *
my_aligned_alloc(size_t align, size_t size) {
#if ALLOC_TYPE == IM_ALIGNED_ALLOC_C11
  return aligned_alloc(align, size);
#elif ALLOC_TYPE == IM_ALIGNED_ALLOC_POSIX
  void *p = NULL;
  if (posix_memalign(&p, align, size)) {
    return NULL;
  }
  return p;
#elif ALLOC_TYPE == IM_ALIGNED_ALLOC_MSVC
  return _aligned_malloc(size, align);
#else
#  error "Unknown ALLOC_TYPE"
#endif
}

void
my_aligned_free(void *p) {
#if ALLOC_TYPE == IM_ALIGNED_ALLOC_C11 || ALLOC_TYPE == IM_ALIGNED_ALLOC_POSIX
  free(p);
#elif ALLOC_TYPE == IM_ALIGNED_ALLOC_MSVC
  _aligned_free(p);
#else
#  error "Unknown ALLOC_TYPE $type " ALLOC_TYPE
#endif
}

int main() {
    double *p = my_aligned_alloc(sizeof(double) * 16, sizeof(double) * 200);
    if (p == NULL) {
       perror("$type aligned allocation failed");
       return 1;
    }
    my_aligned_free(p);
    return 0;
}
EOS
    if (try_compile_and_link($code, run => 1)) {
      return ($type, $defs);
    }
  }
  return ("IM_ALIGNED_ALLOC_NONE", $defs);
}

# probe for a definition that can go before the declaration of
# function that indicates that the function does not return.
#
# perl.h defines something similar, but that's only tested *after* the
# closing ) of the parameter list, but the more portal versions of
# this mechanism go before the the function
sub probe_noreturn {
  for my $nr ("_Noreturn", "[[noreturn]]", "__attribute__((__noreturn__))") {
    print "Noreturn: trying $nr\n" if $VERBOSE;
    my $code = <<CODE;
#include <stdlib.h>
#include <stdnoreturn.h>

void $nr myfunc(void);

int
somefunc(int test) {
  if (test) {
    myfunc();
  }
  else {
    return 0;
  }
}
CODE
    if (try_compile_and_link($code, nolink => 1)) {
      print "Found $nr to mark functions as not returning\n";
      return $nr;
    }
  }

  print "No way to mark functions as noreturn found\n";
  return "";
}

sub usage {
  print STDERR <<EOS;
Usage: $0 [--enable feature1,feature2,...] [other options]
       $0 [--disable feature1,feature2,...]  [other options]
       $0 --help
Possible feature names are:
  FT1 FT2 GIF JPEG PNG T1 TIFF W32
Other options:
  --verbose | -v
    Verbose library probing (or set IM_VERBOSE in the environment)
  --nolog
    Disable logging (or set IM_NOLOG in the environment)
  --incpath dir
    Add to the include search path
  --libpath dir
    Add to the library search path
  --coverage
    Build for coverage testing.  This is incompatible with setting
    OPTIMIZE=... on the command-line.
  --assert
    Build with assertions active.
  --enable=...
  --disable=...
    Enable or disable bundled libraries.
    You can disable all of them with "--enable=none"
    Only one of --enable or --disable can be used
  --assert
    Build with assertions enabled.
  --tracecontext
    Build with trace output for context handling (debugging tool)
  --uselcms
    Build with LittleCMS (requires LittleCMS 2.x installed)
  --Accflags=...
  --Aldflags=...
  --Alddlflags=...
  --Aoptimize=...
    Easily add extra options to ccflags, ldflags, lddlflags or optimize
    without having to extract the existing options.  Cannot be used with
    setting these values with SOMENAME=...
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

my $home;
sub _tilde_expand {
  my ($path) = @_;

  if ($path =~ m!^~[/\\]!) {
    defined $home or $home = $ENV{HOME};
    if (!defined $home && $^O eq 'MSWin32'
       && defined $ENV{HOMEDRIVE} && defined $ENV{HOMEPATH}) {
      $home = $ENV{HOMEDRIVE} . $ENV{HOMEPATH};
    }
    unless (defined $home) {
      $home = eval { (getpwuid($<))[7] };
    }
    defined $home or die "You supplied $path, but I can't find your home directory\n";
    $path =~ s/^~//;
    $path = File::Spec->catdir($home, $path);
  }

  $path;
}

sub _ft1_test_code {
  return <<'CODE';
TT_Engine engine;
TT_Error error;

error = TT_Init_FreeType(&engine);
if (error) {
   printf("FT1: Could not initialize engine\n");
   exit(1);
}

return 0;
CODE
}

sub map_bundled {
  my (@names) = @_;

  @names = map { split /,/ } @names;

  my @outnames;
  for my $name (@names) {
    push @outnames, $name;
    push @outnames, $bundled_names{$name}
      if $bundled_names{$name};
  }

  @outnames;
}

# adapted from Time::HiRes
sub try_compile_and_link {
  my ($c, %args) = @_;

  my ($ok) = 0;
  my ($tmp) = "tmp$$";

  my $obj_ext = $Config{obj_ext} || ".o";
  unlink("$tmp.c", "$tmp$obj_ext");

  if (open(my $tmpc, '>', "$tmp.c")) {
    print $tmpc $c;
    close($tmpc);

    # we don't need this for our probes
    #my $COREincdir = File::Spec->catdir($Config{'archlibexp'}, 'CORE');

    my $cc = $MyConfig{'cc'};
    my $ccflags = $MyConfig{'ccflags'};
    $ccflags .= " $args{ccflags}" if $args{ccflags};
    $ccflags .= " $MyConfig{'optimize'}";
    # we don't need this (yet)
    #. ' ' . "-I$COREincdir" . ' -DPERL_NO_INLINE_FUNCTIONS';

    my $errornull = $VERBOSE ? '' : "2>" . File::Spec->devnull;

    my $libs = $args{libs} || '';
    my $cccmd;
    my $tmp_out;
    if ($args{nolink}) {
      $tmp_out = "$tmp$obj_ext";
      $cccmd = "$cc -c -o $tmp_out $ccflags $tmp.c $errornull";
    }
    else {
      $tmp_out = "$tmp$Config{_exe}";
      $cccmd = "$cc -o $tmp $ccflags $tmp.c $libs $errornull";
    }

    printf "cccmd = $cccmd\n" if $VERBOSE;
    my $res = system($cccmd);
    $ok = defined($res) && $res == 0 && -s $tmp_out;

    if ($ok && !$args{nolink} && !-x $tmp_out) {
      $ok = 0;
    }

    if ( $ok && exists $args{run} && $args{run} ) {
      my $tmp_exe =
        File::Spec->catfile(File::Spec->curdir, $tmp_out);
      my @run = $tmp_exe;
      unshift @run, $Config{run} if $Config{run} && -e $Config{run};
      printf "Running $tmp_exe..." if $VERBOSE;
      if (system(@run) == 0) {
        $ok = 1;
        print " success\n" if $VERBOSE;
      } else {
        $ok = 0;
        my $errno = $? >> 8;
        local $! = $errno;
        printf <<EOF if $args{noisyrun} || $VERBOSE;

*** The test run of '$tmp_exe' failed: status $?
*** (the status means: errno = $errno or '$!')
*** DO NOT PANIC: this just means that *some* functionality will be missing.
EOF
      }
    }
    unlink("$tmp.c", $tmp_out);
  }

  return $ok;
}

sub make_depend {
  my $manifest = maniread();
  # not checking sub-module files
  my @check_files = sort grep /^\w+\.([ch]|xs)$/, keys %$manifest;
  my %inc = map { $_ => 1 } grep /\.h$/, @check_files;

  my %file_deps;
  my %reverse_deps;
  for my $name (@check_files) {
    open my $fh, "<", $name
      or die "Cannot open $name: $!\n";
    while (<$fh>) {
      if (/^\s*#\s*include\s+"(\w+\.h)"\s*$/ && $inc{$1}) {
        $file_deps{$name}{$1} = 1;
        push @{$reverse_deps{$1}}, $name;
      }
    }
  }

  my %final_deps;
  for my $name (grep !/\.h$/, @check_files) {
    my %deps;
    my %checked;
    @deps{keys %{$file_deps{$name}}} = ();
    my %checkme = %deps;
    while (my @check = grep !$checked{$_}, keys %checkme) {
      for my $inc (@check) {
        my @deps = keys %{$file_deps{$inc}};
        @checkme{@deps} = ();
        @deps{@deps} = ();
        ++$checked{$inc};
      }
    }
    (my $obj_name = $name) =~ s/\.(?:c|xs)$/\$(OBJ_EXT)/;
    $final_deps{$obj_name} = join " ", sort keys %deps;
    (my $asm_name = $name) =~ s/\.(?:c|xs)$/.s/;
    $final_deps{$asm_name} = join " ", sort keys %deps;
  }

  return \%final_deps;
}

sub probe_c99_flag {
  my $code = <<'CODE';
#include <stdio.h>

int main() {
  int sum = 55;
  for (int i = 0; i <= 10; ++i) {
    sum -= i;
  }
  return sum;
}
CODE

  # preferably no option, modern gcc (and hopefully other compilers)
  # default to later than C99
  for my $flag ("", qw(-std=c99)) {
    if ($VERBOSE) {
      if ($flag) {
        print "Trying '$flag' to compile C99 code\n";
      }
      else {
        print "Trying to compile C99 code with no extra flags\n";
      }
    }
    if (try_compile_and_link($code, run => 1, ccflags => $flag)) {
      $MyConfig{ccflags} .= " $flag" if $flag;
      return;
    }
  }
  die "OS unsupported: Can't compile C99\n";
}

1;
