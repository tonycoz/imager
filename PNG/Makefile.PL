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

my %opts = 
  (
   NAME => 'Imager::File::PNG',
   VERSION_FROM => 'PNG.pm',
   OBJECT => 'PNG.o impng.o',
   clean => { FILES => 'testout' },
  );

if (eval { ExtUtils::MakeMaker->VERSION('6.46'); 1 }) {
  $opts{LICENSE} = "perl_5";
  $opts{AUTHOR} = 'Tony Cook <tonyc@cpan.org>';
  $opts{ABSTRACT} = 'PNG Image file support for Imager';
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
  print "PNG: building independently\n";
  require Imager::ExtUtils;
  push @inc, Imager::ExtUtils->includes;
  $opts{TYPEMAPS} = [ Imager::ExtUtils->typemap ];

  # Imager required configure through use
  my @Imager_req = ( Imager => "0.90" );
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

# these are mostly needed when pkg-config isn't available
my @alts =
  (
   {
    altname => "v1.6",
    incsuffix => "libpng16",
    libbase => "png16",
   },
   {
    altname => "v1.5",
    incsuffix => "libpng15",
    libbase => "png15",
   },
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
  );

my %probe =
  (
   name => "PNG",
   altname => "Generic",
   pkg => [ qw/libpng libpng16 libpng15 libpng14 libpng12 libpng10/ ],
   inccheck => sub { -e File::Spec->catfile($_[0], "png.h") },
   libbase => "png",
   testcode => _png_test_code(),
   testcodeheaders => [ "png.h", "stdio.h" ],
   incpath => \@incpaths,
   libpath => \@libpaths,
   verbose => $verbose,
   alternatives =>
   [
    @alts,
    {
     altname => "base (+libz)",
     libbase => [ "png", "z" ],
    },
    ( # a static libpng may require libz too
     map +{
	   %$_,
	   altname => "$_->{altname} (+libz)",
	   libbase => [ $_->{libbase}, "z" ],
	  }, @alts
    ),
   ],
  );

my $probe_res = Imager::Probe->probe(\%probe);
if ($probe_res) {
  $IMAGER_LIBS{PNG} = 1;

  push @inc, $probe_res->{INC};
  $opts{LIBS} = $probe_res->{LIBS};
  $opts{DEFINE} = $probe_res->{DEFINE};
  $opts{INC} = "@inc";

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

  if (png_access_version_number() != PNG_LIBPNG_VER) {
     fprintf(stderr, "PNG: Your header version number doesn't match the library version number\n");
     return 1;
  }
return 0;
CODE
}
