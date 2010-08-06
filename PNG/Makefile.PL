#!perl -w
use strict;
use ExtUtils::MakeMaker qw(WriteMakefile WriteEmptyMakefile);
use Getopt::Long;

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
   NAME => 'Imager::File::PNG',
   VERSION_FROM => 'PNG.pm',
   OBJECT => 'PNG.o impng.o',
  );

my @inc;
if ($BUILDING_IMAGER) {
  push @inc, "-I..";
  push @INC, "../lib";
}
else {
  print "PNG: building independently\n";
  require Imager::ExtUtils;
  push @inc, Imager::ExtUtils->includes;
  $opts{TYPEMAPS} = [ Imager::ExtUtils->typemap ];

  # Imager required configure through use
  my @Imager_req = ( Imager => "0.76" );
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
       }
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
   pkg => [ qw/libpng14 libpng12 libpng10 libpng/ ],
   inccheck => sub { -e File::Spec->catfile($_[0], "png.h") },
   libbase => "png",
   testcode => _png_test_code(),
   testcodeheaders => [ "png.h", "stdio.h" ],
  );

my $probe_res = Imager::Probe->probe(\%probe);
if ($probe_res) {
  push @inc, $probe_res->{INC};
  $opts{LIBS} = $probe_res->{LIBS};
  
  $opts{INC} = "@inc";
  
  if ($MM_ver > 6.06) {
    $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>';
    $opts{ABSTRACT} = 'PNG Image file support';
  }
  
  WriteMakefile(%opts);
}
else {
  if ($BUILDING_IMAGER) {
    WriteEmptyMakefile(%opts);
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