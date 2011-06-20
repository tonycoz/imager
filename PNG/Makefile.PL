#!perl -w
use strict;
use ExtUtils::MakeMaker;
use Getopt::Long;
use Config;

my $verbose = $ENV{IM_VERBOSE};
my @libpaths;
my @incpaths;

GetOptions("incpath=s", \@incpaths,
           "libpath=s" => \@libpaths,
           "verbose|v" => \$verbose);

our $BUILDING_IMAGER;
our %IMAGER_LIBS;

my $MM_ver = eval $ExtUtils::MakeMaker::VERSION;

my %opts = 
  (
   NAME => 'Imager::File::PNG',
   VERSION_FROM => 'PNG.pm',
   OBJECT => 'PNG.o impng.o',
   clean => { FILES => 'testout' },
  );

my @inc;
if ($BUILDING_IMAGER) {
  push @inc, "-I..";
  unshift @INC, "../lib";
}
else {
  unshift @INC, "inc";
  print "PNG: building independently\n";
  require Imager::ExtUtils;
  push @inc, Imager::ExtUtils->includes;
  $opts{TYPEMAPS} = [ Imager::ExtUtils->typemap ];

  # Imager required configure through use
  my @Imager_req = ( Imager => "0.78" );
  if ($MM_ver >= 6.46) {
    $opts{META_MERGE} =
      {
       configure_requires => 
       {
	@Imager_req,
       },
       build_requires => 
       {
	@Imager_req,
	"Test::More" => "0.47",
       },
       resources =>
       {
	homepage => "http://imager.perl.org/",
	repository => "git://git.imager.perl.org/imager.git",
	bugtracker => "http://rt.cpan.org/NoAuth/Bugs.html?Dist=Imager",
       },
      };
    $opts{PREREQ_PM} =
      {
       @Imager_req,
      };
  }
}

require Imager::Probe;

my %probe =
  (
   name => "PNG",
   altname => "Generic",
   pkg => [ qw/libpng14 libpng12 libpng10 libpng/ ],
   inccheck => sub { -e File::Spec->catfile($_[0], "png.h") },
   libbase => "png",
   testcode => _png_test_code(),
   testcodeheaders => [ "png.h", "stdio.h" ],
   incpath => \@incpaths,
   libpath => \@libpaths,
   alternatives =>
   [
    {
     altname => "v1.4",
     incsuffix => "libpng14",
     libbase => "png14",
    },
    {
     altname => "v1.2",
     incsuffix => "libpng12",
     libbase => "png12",
    },
    {
     altname => "v1.0",
     incsuffix => "libpng10",
     libbase => "png10",
    },
   ],
  );

my $probe_res = Imager::Probe->probe(\%probe);
if ($probe_res) {
  $IMAGER_LIBS{PNG} = 1;

  push @inc, $probe_res->{INC};
  $opts{LIBS} = $probe_res->{LIBS};
  $opts{DEFINE} = $probe_res->{DEFINE};
  $opts{INC} = "@inc";
  
  if ($MM_ver > 6.06) {
    $opts{AUTHOR} = 'Tony Cook <tonyc@cpan.org>';
    $opts{ABSTRACT} = 'PNG Image file support';
  }
  
  WriteMakefile(%opts);
}
else {
  $IMAGER_LIBS{PNG} = 0;

  if ($BUILDING_IMAGER) {
    ExtUtils::MakeMaker::WriteEmptyMakefile(%opts);
  }
  else {
    # fail in good way
    die "OS unsupported: PNG libraries or headers not found\n";
  }
}

sub _png_test_code {
  return <<'CODE';

fprintf(stderr, "PNG: library version %ld, header version %ld\n", (long)png_access_version_number(),  (long)PNG_LIBPNG_VER);
return 0;
CODE
}
