#!perl -w
use strict;
use ExtUtils::MakeMaker qw(WriteMakefile WriteEmptyMakefile);
use Getopt::Long;
use Config;

my $verbose = $ENV{IM_VERBOSE};
my @libpaths;
my @incpaths;

GetOptions("incpath=s", \@incpaths,
           "libpath=s" => \@libpaths,
           "verbose|v" => \$verbose);

our $BUILDING_IMAGER;

my $MM_ver = eval $ExtUtils::MakeMaker::VERSION;

my %opts = 
  (
   NAME => 'Imager::File::GIF',
   VERSION_FROM => 'GIF.pm',
   OBJECT => 'GIF.o imgif.o',
  );

my @inc;
if ($BUILDING_IMAGER) {
  unshift @inc, "-I..";
  unshift @INC, "../lib";
}
else {
  unshift @INC, "inc";
  print "GIF: building independently\n";
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
	 url => "http://imager.perl.org/svn/trunk/Imager-File-GIF",
	 web => "http://imager.perl.org/svnweb/public/browse/trunk/Imager-File-GIF",
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
   name => "GIF",
   inccheck => sub { -e File::Spec->catfile($_[0], "gif_lib.h") },
   libbase => "gif",
   testcode => _gif_test_code(),
   testcodeheaders => [ "gif_lib.h", "stdio.h" ],
   incpath => join($Config{path_sep}, @incpaths),
   libpath => join($Config{path_sep}, @libpaths),
  );

my $probe_res = Imager::Probe->probe(\%probe);
if ($probe_res) {
  push @inc, $probe_res->{INC};
  $opts{LIBS} = $probe_res->{LIBS};
  
  $opts{INC} = "@inc";
  
  if ($MM_ver > 6.06) {
    $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>';
    $opts{ABSTRACT} = 'GIF Image file support';
  }
  
  WriteMakefile(%opts);
}
else {
  if ($BUILDING_IMAGER) {
    WriteEmptyMakefile(%opts);
  }
  else {
    # fail in good way
    die "OS unsupported: GIF libraries or headers not found\n";
  }
}

sub _gif_test_code {
  return <<'CODE';
/* TODO: test DGifOpen() and processing with callbacks */
GifFileType *gf;
const char vers[] = GIF_LIB_VERSION;
const char *versp = vers;
int ver_maj;
int ver_min;
fprintf(stderr, "GIF: header version '%s'\n", GIF_LIB_VERSION);
gf=DGifOpenFileName("testimg/expected.gif");
if (!gf) {
  fprintf(stderr, "GIF: Cannot open testimg/expected.gif\n");
  return 1;
}
if (gf->SWidth != 16 || gf->SHeight != 16) {
  fprintf(stderr, "GIF: bad screen description (%d x %d)\n", gf->SWidth, gf->SHeight);
  return 1;
}
/* crashes in older versions of giflib */
EGifSetGifVersion("89a");
EGifSetGifVersion("87a");

/* skip the " Version " */
while (*versp && (*versp < '0' || *versp > '9'))
  ++versp;
if (!*versp) {
  fprintf(stderr, "GIF: Cannot find version number in '%s'\n", vers);
  return 1;
}
ver_maj = 0;
while (*versp && *versp >= '0' && *versp <= '9') {
  ver_maj *= 10;
  ver_maj += *versp++ - '0';
}
if (*versp != '.' || versp[1] < '0' || versp[1] > '9') {
  fprintf(stderr, "Cannot parse major version number in '%s'\n", vers);
  return 1;
}
++versp; /* skip '.' */
ver_min = 0;
while (*versp && *versp >= '0' && *versp <= '9') {
  ver_min *= 10;
  ver_min += *versp++ - '0';
}
if (ver_maj < 4) {
  fprintf(stderr, "GIF: gif lib version 3 is no longer supported\n");
  return 1;
}
if (ver_maj == 4 && ver_min < 1) {
  fprintf(stderr, "GIF: you need at least giflib 4.1\n");
  return 1;
}
fprintf(stderr, "GIF: Major %d, Minor %d\n", ver_maj, ver_min);
return 0;
CODE
}