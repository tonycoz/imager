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
   NAME => 'Imager::Font::W32',
   VERSION_FROM => 'W32.pm',
   OBJECT => 'W32.o win32.o',
   clean => { FILES => 'testout' },
  );

if (eval { ExtUtils::MakeMaker->VERSION('6.46'); 1 }) {
  $opts{LICENSE} = "perl_5";
  $opts{AUTHOR} = 'Tony Cook <tonyc@cpan.org>';
  $opts{ABSTRACT} = 'Win32 font driver for Imager';
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
       url => "git://git.imager.perl.org/imager.git",
       web => "http://git.imager.perl.org/imager.git",
      },
      bugtracker =>
      {
       web => "http://rt.cpan.org/NoAuth/Bugs.html?Dist=Imager",
       mailto => 'bug-Imager@rt.cpan.org',
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
  print "Win32: building independently\n";
  require Imager::ExtUtils;
  push @inc, Imager::ExtUtils->includes;
  $opts{TYPEMAPS} = [ Imager::ExtUtils->typemap ];

  # Imager required configure through use
  my @Imager_req = ( Imager => "0.95" );
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
   name => "Win32",
   inccheck => sub { -e File::Spec->catfile($_[0], "windows.h") },
   libbase => "gdi32",
   testcode => _win32_test_code(),
   testcodeheaders => [ "stdio.h", "string.h", "windows.h" ],
   incpath => \@incpaths,
   libpath => \@libpaths,
   verbose => $verbose,
  );

my $probe_res = Imager::Probe->probe(\%probe);
if ($probe_res) {
  $IMAGER_LIBS{Win32} = 1;

  push @inc, $probe_res->{INC};
  $opts{LIBS} = $probe_res->{LIBS};
  $opts{DEFINE} = $probe_res->{DEFINE};
  $opts{INC} = "@inc";

  WriteMakefile(%opts);
}
else {
  $IMAGER_LIBS{Win32} = 0;

  if ($BUILDING_IMAGER) {
    ExtUtils::MakeMaker::WriteEmptyMakefile(%opts);
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
