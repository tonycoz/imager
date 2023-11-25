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

my $define = "";
my $fp_rep = unpack("H*", pack("f", 1.25));
if ($fp_rep eq "0000a03f" || $fp_rep eq "3fa00000") {
  $define = "-DIEEEFP_TYPES";
}

my %opts = 
  (
   NAME => 'Imager::File::TIFF',
   VERSION_FROM => 'TIFF.pm',
   OBJECT => 'TIFF.o imtiff.o',
   DEFINE => $define,
   clean => { FILES => 'testout' },
  );

if (eval { ExtUtils::MakeMaker->VERSION('6.46'); 1 }) {
  $opts{LICENSE} = "perl_5";
  $opts{AUTHOR} = 'Tony Cook <tonyc@cpan.org>';
  $opts{ABSTRACT} = 'TIFF image file support for Imager';
  $opts{META_MERGE} =
    {
     'meta-spec' =>
     {
      version => "2",
      url => "https://metacpan.org/pod/CPAN::Meta::Spec",
     },
     resources =>
     {
      homepage => "http://imager.perl.org/",
      repository =>
      {
       type => "git",
       url => "git://github.com/tonycoz/imager.git",
       web => "https://github.com/tonycoz/imager.git",
      },
      bugtracker =>
      {
       web => "https://github.com/tonycoz/imager/issues",
      },
     },
    };
}

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
  my @Imager_req = ( Imager => "0.94" );
  if (eval { ExtUtils::MakeMaker->VERSION('6.46'); 1 }) {
    $opts{META_MERGE}{prereqs} =
      {
       configure =>
       {
	requires =>
	{
	 @Imager_req,
	},
       },
       build =>
       {
	requires =>
	{
	 @Imager_req,
	 "Test::More" => "0.47",
	}
       },
       runtime =>
       {
	requires =>
	{
	 @Imager_req,
	}
       },
       test =>
       {
	requires =>
	{
	 "Test::More" => "0.47",
	}
       },
      };
    $opts{PREREQ_PM} =
      {
       @Imager_req,
       XSLoader => 0,
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
   verbose => $verbose,
   pkg => [ qw(libtiff-5 libtiff-4) ],
  );

my $probe_res = Imager::Probe->probe(\%probe);
if ($probe_res) {
  $IMAGER_LIBS{TIFF} = 1;

  push @inc, $probe_res->{INC};
  $opts{LIBS} = $probe_res->{LIBS};
  $opts{DEFINE} .= " $probe_res->{DEFINE}";
  $opts{INC} = "@inc";

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

fprintf(stderr, "TIFF: library version %.*s, header version %ld\n", ver_len, ver_start, (long)TIFFLIB_VERSION);
if (TIFFLIB_VERSION < 20191103) {
  fprintf(stderr, "TIFF: libtiff 4.1.0 or later required\n");
  return 1;
}
return 0;
CODE
}
