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
GetOptions("help" => \$help,
           "enable=s" => \@enable,
           "disable=s" => \@disable,
           "incpath=s", \@incpaths,
           "libpath=s" => \@libpaths,
           "verbose|v" => \$VERBOSE,
           "nolog" => \$NOLOG,
	   'coverage' => \$coverage,
	   "assert|a" => \$assert,
	   "tracecontext" => \$trace_context);

setenv();

if ($ENV{AUTOMATED_TESTING}) {
  $assert = 1;
}

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
	      perlio.o);

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
   clean          => { FILES=>'testout rubthru.c scale.c conv.c  filters.c gaussian.c render.c rubthru.c' },
   PM             => gen_PM(),
   PREREQ_PM      =>
   { 
    'Test::More' => 0.47,
    'Scalar::Util' => 1.00,
    'XSLoader'    => 0,
   },
   TYPEMAPS       => \@typemaps,
   test =>        { TESTS => $tests },
  );

if ($coverage) {
    if ($Config{gccversion}) {
	push @ARGV, 'OPTIMIZE=-ftest-coverage -fprofile-arcs -g';
	$opts{dynamic_lib} = { OTHERLDFLAGS => '-ftest-coverage -fprofile-arcs' };
    }
    else {
	die "Don't know the coverage C flags for your compiler\n";
    }
}

# eval to prevent warnings about versions with _ in them
my $MM_ver = eval $ExtUtils::MakeMaker::VERSION;
if ($MM_ver > 6.06) {
  $opts{AUTHOR} = 'Tony Cook <tonyc@cpan.org>, Arnar M. Hrafnkelsson';
  $opts{ABSTRACT} = 'Perl extension for Generating 24 bit Images';
}

if ($MM_ver >= 6.46) {
  $opts{META_MERGE} =
    {
     recommends =>
     {
      "Parse::RecDescent" => 0
     },
     license => "perl",
     dynamic_config => 1,
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
      repository => "git://git.imager.perl.org/imager.git",
      bugtracker => "http://rt.cpan.org/NoAuth/Bugs.html?Dist=Imager",
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
  if ($Config{gccversion} && $Config{gccversion} =~ /^([0-9]+)/ && $1 > 3) {
    print CONFIG <<EOS;
/*

Compiler supports the GCC __attribute__((format...)) syntax.

*/

#define IMAGER_FORMAT_ATTR 1

EOS
  }

  if ($Config{d_snprintf}) {
    print CONFIG <<EOS;
/* We can use snprintf() */
#define IMAGER_SNPRINTF 1

EOS
  }

  if ($Config{d_vsnprintf}) {
    print CONFIG <<EOS;
/* We can use vsnprintf() */
#define IMAGER_VSNPRINTF 1

EOS
  }

  print CONFIG <<EOS;
/*
 Type and format code for formatted output as with printf.

 This is intended for formatting i_img_dim values.
*/
typedef $Config{ivtype} i_dim_format_t;
#define i_DF $Config{ivdformat}
EOS

  print CONFIG "\n#endif\n";
  close CONFIG;
}

sub usage {
  print STDERR <<EOS;
Usage: $0 [--enable feature1,feature2,...] [other options]
       $0 [--disable feature1,feature2,...]  [other options]
       $0 --help
Possible feature names are:
  T1-fonts
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
    Build for coverage testing.
  --assert
    Build with assertions active.
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

1;
