
use ExtUtils::MakeMaker;
use Cwd;
use Config;

$lext=$Config{'so'};   # Get extensions of libraries
$aext=$Config{'_a'};

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
init();       # initialize global data
pathcheck();  # Check if directories exist

# Pick what libraries are used
if ($MANUAL) {
  manual();
} else {
  automatic();
  if (exists $ENV{IM_ENABLE}) {
    my %en = map { $_, 1 } split ' ', $ENV{IM_ENABLE};
    for my $key (keys %formats) {
      delete $formats{$key} unless $en{$key};
    }
  }
}

# Make sure there isn't a clash between the gif libraries.
gifcheck();

for $frm(values %formats) {
  $F_DEFINE .= ' -D'.$frm->{def};
  $F_LIBS   .= ' '  .$frm->{libfiles};
  $F_OBJECT .= ' '  .$frm->{objfiles};
}

$F_INC  = join(" ",map { (exists $definc{$_})?'':'-I'.$_ } @incs);
$F_LIBS = join(" ",map { '-L'.$_ } @libs).' '.$F_LIBS;

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
       'DEFINE'       => "$F_DEFINE $EXTDEF $OSDEF $CFLAGS",
       'INC'          => "$DFLAGS $F_INC",
       'OBJECT'       => join(' ', @objs, $F_OBJECT),
       clean          => { FILES=>'testout' },
      );

if ($] ge '5.005') {
  $opts{AUTHOR} = 'Arnar M. Hrafnkelsson, addi@umich.edu';
  $opts{ABSTRACT} = 'Perl extension for Generating 24 bit Images';
}

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
';
}

# manual configuration of helper libraries

sub manual {
  print <<EOF;

      Please answer the following questions about
      which formats are avaliable on your computer

press <return> to continue
EOF

  <STDIN>; # eat one return

  for $frm(sort { $formats{$b}{order} <=> $formats{$a}{order} } keys %formats) {
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

  $F_DEFINE .= "-DIM_GIFMAJOR=$major -DIM_GIFMINOR=$minor";
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
  my $libchk=$formats{$frm}{'libcheck'};
  my $incchk=$formats{$frm}{'inccheck'};

  my @l;
  for my $lp (@libs) {
    push(@l, gd($lp,$libchk));
  }

  my @i;
  for my $ip (@incs) {
    push(@i, gd($ip,$incchk));
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
  @incs=(qw(/sw/include /usr/include/freetype2 /usr/local/include/freetype2 /usr/include /usr/local/include /usr/include/freetype /usr/local/include/freetype), split /\Q$Config{path_sep}/, $INCPATH );
  @libs=(qw(/sw/lib), split(/\Q$Config{path_sep}/,$LIBPATH) , split(/ /, $Config{'libpth'}));
  if ($^O =~ /win32/i && $Config{cc} =~ /\bcl\b/i) {
    push(@incs, split /;/, $ENV{INCLUDE}) if exists $ENV{INCLUDE};
    push(@libs, split /;/, $ENV{LIB}) if exists $ENV{LIB};
  }
  if ($^O eq 'cygwin') {
    push(@libs, '/usr/lib/w32api') if -d '/usr/lib/w32api';
    push(@incs, '/usr/include/w32api') if -d '/usr/lib/w32api';
  }

  $formats{'jpeg'}={
		    order=>'21',
		    def=>'HAVE_LIBJPEG',
		    inccheck=>sub { $_[0] eq 'jpeglib.h' },
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
		    inccheck=>sub { $_[0] eq 'tiffio.h' },
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
		   inccheck=>sub { $_[0] eq 'png.h' },
		   libcheck=>sub { $_[0] eq "libpng$aext" or $_[0] eq "libpng.$lext" },
		   libfiles=>'-lpng -lz',
		   objfiles=>'png.o',
		   docs=>q{
			   Png stands for Portable Network Graphics and is intended as
			   a replacement for gif on the web. It is patent free and
			   is recommended by the w3c, you need libpng to use these formats}
		  };

  $formats{'gif'}={
		   order=>'20',
		   def=>'HAVE_LIBGIF',
		   inccheck=>sub { $_[0] eq 'gif_lib.h' },
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
		     inccheck=>sub { $_[0] eq 'gif_lib.h' },
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
			inccheck=>sub { $_[0] eq 't1lib.h' },
			libcheck=>sub { $_[0] eq "libt1$aext" or $_[0] eq "libt1.$lext" },
			libfiles=>'-lt1',
			objfiles=>'',
			docs=>q{
				postscript t1 fonts are scalable fonts. They can include 
				ligatures and kerning information and generally yield good
				visual quality. We depend on libt1 to rasterize the fonts
				for use in images.}
		       };

  $formats{'TT-fonts'}={
			order=>'31',
			def=>'HAVE_LIBTT',
			inccheck=>sub { $_[0] eq 'freetype.h' },
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
		     inccheck=>sub { lc $_[0] eq 'windows.h' },
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
                           inccheck=>sub { lc $_[0] eq 'ft2build.h' },
                           libcheck=>sub { $_[0] eq "libfreetype$aext" or $_[0] eq "libfreetype.$lext" },
                           libfiles=>'-lfreetype',
                           objfiles=>'freetyp2.o',
                           docs=><<DOCS
Freetype 2 supports both Truetype and Type 1 fonts, both of which are
scalable.
DOCS
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
  else { $EXTDEF.=' -DIMAGER_LOG'; }

  if ($DEBUG_MALLOC) {
    $EXTDEF.=' -DIMAGER_DEBUG_MALLOC';
    print "Malloc debugging enabled\n";
  }

}
