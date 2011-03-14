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
   NAME => 'Imager::File::TIFF',
   VERSION_FROM => 'TIFF.pm',
   OBJECT => 'TIFF.o imtiff.o',
   clean => { FILES => 'testout' },
  );

my @inc;
if ($BUILDING_IMAGER) {
  push @inc, "-I..";
  unshift @INC, "../lib";
}
else {
  unshift @INC, "inc";
  print "TIFF: building independently\n";
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
	repository =>
	{
	 url => "http://imager.perl.org/svn/trunk/Imager/TIFF",
	 web => "http://imager.perl.org/svnweb/public/browse/trunk/Imager/TIFF",
	 type => "svn",
	},
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
   name => "TIFF",
   inccheck => sub { -e File::Spec->catfile($_[0], "tiffio.h") },
   libbase => "tiff",
   testcode => _tiff_test_code(),
   testcodeheaders => [ "tiffio.h", "stdio.h", "string.h" ],
   incpath => \@incpaths,
   libpath => \@libpaths,
  );

my $probe_res = Imager::Probe->probe(\%probe);
if ($probe_res) {
  $IMAGER_LIBS{TIFF} = 1;

  push @inc, $probe_res->{INC};
  $opts{LIBS} = $probe_res->{LIBS};
  $opts{DEFINE} = $probe_res->{DEFINE};
  $opts{INC} = "@inc";

  if ($MM_ver > 6.06) {
    $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>';
    $opts{ABSTRACT} = 'TIFF image file support for Imager';
  }
  
  WriteMakefile(%opts);
}
else {
  $IMAGER_LIBS{TIFF} = 0;

  if ($BUILDING_IMAGER) {
    ExtUtils::MakeMaker::WriteEmptyMakefile(%opts);
  }
  else {
    # fail in good way
    die "OS unsupported: TIFF libraries or headers not found\n";
  }
}

sub _tiff_test_code {
  return <<'CODE';
static const char ver_base[] = ", Version ";
static const size_t ver_base_len = sizeof(ver_base) - 1;
const char *ver_str = TIFFGetVersion();
const char *ver_start = strstr(ver_str, ver_base);
const char *ver_end;
int ver_len;

if (ver_start && ver_start[ver_base_len] >= '3' && ver_start[ver_base_len] < '9') {
  ver_start += ver_base_len;
  ver_end = ver_start;
  while (*ver_end && (*ver_end == '.' || *ver_end >= '0' && *ver_end <= '9'))
    ++ver_end;
  ver_len = ver_end - ver_start;
}
else {
  ver_start = "(unknown)";
  ver_len = strlen(ver_start);
}

fprintf(stderr, "TIFF: library version %.*s, header version %ld\n", ver_len, ver_start, TIFFLIB_VERSION);
if (TIFFLIB_VERSION == 20090820) {
  fprintf(stderr, "TIFF: this appears to be libtiff 3.9.0 which introduced a serious bug\n");
  fprintf(stderr, "TIFF: please install 3.9.1\n");
  return 1;
}
return 0;
CODE
}
