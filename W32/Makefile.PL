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
   NAME => 'Imager::Font::W32',
   VERSION_FROM => 'W32.pm',
   OBJECT => 'W32.o win32.o',
   clean => { FILES => 'testout' },
  );

my @inc;
if ($BUILDING_IMAGER) {
  push @inc, "-I..";
  unshift @INC, "../lib";
}
else {
  unshift @INC, "inc";
  print "Win32: building independently\n";
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
	 url => "http://imager.perl.org/svn/trunk/Imager/W32",
	 web => "http://imager.perl.org/svnweb/public/browse/trunk/Imager/W32",
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
   name => "Win32",
   inccheck => sub { -e File::Spec->catfile($_[0], "windows.h") },
   libbase => "gdi32",
   testcode => _win32_test_code(),
   testcodeheaders => [ "stdio.h", "string.h", "windows.h" ],
   incpath => \@incpaths,
   libpath => \@libpaths,
  );

my $probe_res = Imager::Probe->probe(\%probe);
if ($probe_res) {
  push @inc, $probe_res->{INC};
  $opts{LIBS} = $probe_res->{LIBS};

  $opts{INC} = "@inc";

  if ($MM_ver > 6.06) {
    $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>';
    $opts{ABSTRACT} = 'Win32 font file support for Imager';
  }
  
  WriteMakefile(%opts);
}
else {
  if ($BUILDING_IMAGER) {
    WriteEmptyMakefile(%opts);
  }
  else {
    # fail in good way
    die "OS unsupported: Win32 libraries or headers not found\n";
  }
}

sub _win32_test_code {
  return <<'CODE';
HDC dc = GetDC(NULL);
HDC bmpDc = CreateCompatibleDC(dc);
DeleteDC(bmpDc);
ReleaseDC(NULL, dc);
return 0;
CODE
}