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
   NAME => 'Imager::File::JPEG',
   VERSION_FROM => 'JPEG.pm',
   OBJECT => 'JPEG.o imjpeg.o imexif.o',
   clean => { FILES => 'testout' },
  );

my @inc;
if ($BUILDING_IMAGER) {
  push @inc, "-I..";
  unshift @INC, "../lib";
}
else {
  unshift @INC, "inc";
  print "JPEG: building independently\n";
  require Imager::ExtUtils;
  push @inc, Imager::ExtUtils->includes;
  $opts{TYPEMAPS} = [ Imager::ExtUtils->typemap ];

  # Imager required configure through use
  my @Imager_req = ( Imager => "0.77" );
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
	 url => "http://imager.perl.org/svn/trunk/Imager-File-JPEG",
	 web => "http://imager.perl.org/svnweb/public/browse/trunk/Imager-File-JPEG",
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
   name => "JPEG",
   inccheck => sub { -e File::Spec->catfile($_[0], "jpeglib.h") },
   libbase => "jpeg",
   testcode => _jpeg_test_code(),
   testcodeheaders => [ "stdio.h", "stddef.h", "jpeglib.h" ],
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
    $opts{ABSTRACT} = 'JPEG Image file support';
  }
  
  WriteMakefile(%opts);
}
else {
  if ($BUILDING_IMAGER) {
    WriteEmptyMakefile(%opts);
  }
  else {
    # fail in good way
    die "OS unsupported: JPEG libraries or headers not found\n";
  }
}

sub _jpeg_test_code {
  return <<'CODE';
struct jpeg_decompress_struct cinfo;
struct jpeg_error_mgr jerr;

cinfo.err = jpeg_std_error(&jerr);
jpeg_create_decompress(&cinfo);

return 0;
CODE
}
