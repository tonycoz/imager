package Imager::Probe;
use strict;
use File::Spec;
use Config;

my @alt_transfer = qw/altname incsuffix libbase/;

sub probe {
  my ($class, $req) = @_;

  $req->{verbose} ||= $ENV{IM_VERBOSE};

  my $name = $req->{name};
  my $result;
  if ($req->{code}) {
    $result = _probe_code($req);
  }
  if (!$result && $req->{pkg}) {
    $result = _probe_pkg($req);
  }
  if (!$result && $req->{inccheck} && ($req->{libcheck} || $req->{libbase})) {
    $req->{altname} ||= "main";
    $result = _probe_check($req);
  }
  if (!$result && $req->{alternatives}) {
  ALTCHECK:
    my $index = 1;
    for my $alt (@{$req->{alternatives}}) {
      $req->{altname} = $alt->{altname} || "alt $index";
      $req->{verbose}
	and print "$req->{name}: Trying alternative $index\n";
      my %work = %$req;
      for my $key (@alt_transfer) {
	exists $alt->{$key} and $work{$key} = $alt->{$key};
      }
      $result = _probe_check(\%work)
	and last;
      ++$index;
    }
  }

  if (!$result && $req->{testcode}) {
    $result = _probe_fake($req);
  }
  $result or return;

  if ($req->{testcode}) {
    $result = _probe_test($req, $result);
  }

  $result or return;

  return $result;
}

sub _probe_code {
  my ($req) = @_;

  my $code = $req->{code};
  my @probes = ref $code eq "ARRAY" ? @$code : $code;

  my $result;
  for my $probe (@probes) {
    $result = $probe->($req)
      and return $result;
  }

  return;
}

sub is_exe {
  my ($name) = @_;

  my @exe_suffix = $Config{_exe};
  if ($^O eq 'MSWin32') {
    push @exe_suffix, qw/.bat .cmd/;
  }

  for my $dir (File::Spec->path) {
    for my $suffix (@exe_suffix) {
      -x File::Spec->catfile($dir, "$name$suffix")
	and return 1;
    }
  }

  return;
}

sub _probe_pkg {
  my ($req) = @_;

  # Setup pkg-config's environment variable to search non-standard paths
  # which may be provided by --libdirs.
  my @pkgcfg_paths = map { "$_/pkgconfig" } _lib_paths( $req );
  push @pkgcfg_paths, $ENV{ 'PKG_CONFIG_PATH' } if $ENV{ 'PKG_CONFIG_PATH' };

  local $ENV{ 'PKG_CONFIG_PATH' } = join $Config{path_sep}, @pkgcfg_paths;

  is_exe('pkg-config') or return;
  my $redir = $^O eq 'MSWin32' ? '' : '2>/dev/null';

  my @pkgs = @{$req->{pkg}};
  for my $pkg (@pkgs) {
    if (!system("pkg-config $pkg --exists $redir")) {
      # if we find it, but the following fail, then pkg-config is too
      # broken to be useful
      my $cflags = `pkg-config $pkg --cflags`
	and !$? or return;

      my $lflags = `pkg-config $pkg --libs`
	and !$? or return;

      my $defines = '';
      $cflags =~ s/(-D\S+)/$defines .= " $1"; ''/ge;

      chomp $cflags;
      chomp $lflags;
      print "$req->{name}: Found via pkg-config $pkg\n";
      return
	{
	 INC => $cflags,
	 LIBS => $lflags,
	 DEFINE => $defines,
	};
    }
  }

  print "$req->{name}: Not found via pkg-config\n";

  return;
}

sub _probe_check {
  my ($req) = @_;

  my $libcheck = $req->{libcheck};
  my $libbase = $req->{libbase};
  if (!$libcheck && $req->{libbase}) {
    # synthesize a libcheck
    my $lext=$Config{'so'};   # Get extensions of libraries
    my $aext=$Config{'_a'};
    $libcheck = sub {
      -e File::Spec->catfile($_[0], "lib$libbase$aext")
	|| -e File::Spec->catfile($_[0], "lib$libbase.$lext")
      };
  }

  my $found_libpath;
  my @lib_search = _lib_paths($req);
  print "$req->{name}: Searching directories for libraries:\n"
    if $req->{verbose};
  for my $path (@lib_search) {
    print "$req->{name}:   $path\n" if $req->{verbose};
    if ($libcheck->($path)) {
      print "$req->{name}: Found!\n" if $req->{verbose};
      $found_libpath = $path;
      last;
    }
  }

  my $found_incpath;
  my $inccheck = $req->{inccheck};
  my @inc_search = _inc_paths($req);
  print "$req->{name}: Searching directories for headers:\n"
    if $req->{verbose};
  for my $path (@inc_search) {
    print "$req->{name}:   $path\n" if $req->{verbose};
    if ($inccheck->($path)) {
      print "$req->{name}: Found!\n" if $req->{verbose};
      $found_incpath = $path;
      last;
    }
  }

  my $alt = "";
  if ($req->{altname}) {
    $alt = " $req->{altname}:";
  }
  print "$req->{name}:$alt includes ", $found_incpath ? "" : "not ",
    "found - libraries ", $found_libpath ? "" : "not ", "found\n";

  $found_libpath && $found_incpath
    or return;

  my @libs = "-L$found_libpath";
  if ($req->{libopts}) {
    push @libs, $req->{libopts};
  }
  elsif ($libbase) {
    push @libs, "-l$libbase";
  }
  else {
    die "$req->{altname}: inccheck but no libbase or libopts";
  }

  return
    {
     INC => "-I$found_incpath",
     LIBS => "@libs",
     DEFINE => "",
    };
}

sub _probe_fake {
  my ($req) = @_;

  # the caller provided test code, and the compiler may look in
  # places we don't, see Imager-Screenshot ticket 56793,
  # so fake up a result so the test code can 
  my $lopts;
  if ($req->{libopts}) {
    $lopts = $req->{libopts};
  }
  elsif (defined $req->{libbase}) {
    # might not need extra libraries, eg. Win32 perl already links
    # everything
    $lopts = $req->{libbase} ? "-l$req->{libbase}" : "";
  }
  if (defined $lopts) {
    print "$req->{name}: Checking if the compiler can find them on its own\n";
    return
      {
       INC => "",
       LIBS => $lopts,
       DEFINE => "",
      };
  }
  else {
    print "$req->{name}: Can't fake it - no libbase or libopts\n"
      if $req->{verbose};
    return;
  }
}

sub _probe_test {
  my ($req, $result) = @_;

  require Devel::CheckLib;
  # setup LD_RUN_PATH to match link time
  my ($extra, $bs_load, $ld_load, $ld_run_path) =
    ExtUtils::Liblist->ext($result->{LIBS}, $req->{verbose});
  local $ENV{LD_RUN_PATH};

  if ($ld_run_path) {
    print "Setting LD_RUN_PATH=$ld_run_path for $req->{name} probe\n"
      if $req->{verbose};
    $ENV{LD_RUN_PATH} = $ld_run_path;
  }
  my $good =
    Devel::CheckLib::check_lib
	(
	 debug => $req->{verbose},
	 LIBS => [ $result->{LIBS} ],
	 INC => $result->{INC},
	 header => $req->{testcodeheaders},
	 function => $req->{testcode},
	 prologue => $req->{testcodeprologue},
	);
  unless ($good) {
    print "$req->{name}: Test code failed: $@";
    return;
  }

  print "$req->{name}: Passed code check\n";
  return $result;
}

sub _lib_paths {
  my ($req) = @_;

  return _paths
    (
     $ENV{IM_LIBPATH},
     $req->{libpath},
     (
      map { split ' ' }
      grep $_,
      @Config{qw/loclibpth libpth libspath/}
     ),
     $^O eq "MSWin32" ? $ENV{LIB} : "",
     $^O eq "cygwin" ? "/usr/lib/w32api" : "",
     "/usr/lib",
     "/usr/local/lib",
    );
}

sub _inc_paths {
  my ($req) = @_;

  my @paths = _paths
    (
     $ENV{IM_INCPATH},
     $req->{incpath},
     $^O eq "MSWin32" ? $ENV{INCLUDE} : "",
     $^O eq "cygwin" ? "/usr/include/w32api" : "",
     (
      map { split ' ' }
      grep $_,
      @Config{qw/locincpth incpath/}
     ),
     "/usr/include",
     "/usr/local/include",
    );

  if ($req->{incsuffix}) {
    @paths = map File::Spec->catdir($_, $req->{incsuffix}), @paths;
  }

  return @paths;
}

sub _paths {
  my (@in) = @_;

  my @out;

  # expand any array refs
  @in = map { ref() ? @$_ : $_ } @in;

  for my $path (@in) {
    $path or next;
    $path = _tilde_expand($path);

    push @out, grep -d $_, split /\Q$Config{path_sep}/, $path;
  }

  return @out;
}

my $home;
sub _tilde_expand {
  my ($path) = @_;

  if ($path =~ m!^~[/\\]!) {
    defined $home or $home = $ENV{HOME};
    if (!defined $home && $^O eq 'MSWin32'
       && defined $ENV{HOMEDRIVE} && defined $ENV{HOMEPATH}) {
      $home = $ENV{HOMEDRIVE} . $ENV{HOMEPATH};
    }
    unless (defined $home) {
      $home = eval { (getpwuid($<))[7] };
    }
    defined $home or die "You supplied $path, but I can't find your home directory\n";
    $path =~ s/^~//;
    $path = File::Spec->catdir($home, $path);
  }

  return $path;
}

1;

__END__

=head1 NAME

Imager::Probe - hot needle of inquiry for libraries

=head1 SYNOPSIS

  require Imager::Probe;

  my %probe = 
    (
     # short name of what we're looking for (displayed to user)
     name => "FOO",
     # pkg-config lookup
     pkg => [ qw/name1 name2 name3/ ],
     # perl subs that probe for the library
     code => [ \&foo_probe1, \&foo_probe2 ],
     # or just: code => \&foo_probe,
     inccheck => sub { ... },
     libcheck => sub { ... },
     # search for this library if libcheck not supplied
     libbase => "foo",
     # library link time options, uses libbase to build options otherwise
     libopts => "-lfoo",
     # C code to check the library is sane
     testcode => "...",
     # header files needed
     testcodeheaders => [ "stdio.h", "foo.h" ],
    );
  my $result = Imager::Probe->probe(\%probe)
    or print "Foo library not found: ",Imager::Probe->error;

=head1 DESCRIPTION

Does the probes that were hidden in Imager's F<Makefile.PL>, pulled
out so the file format libraries can be externalized.

The return value is either nothing if the probe fails, or a hash
containing:

=over

=item *

C<INC> - C<-I> and other C options

=item *

C<LIBS> - C<-L>, C<-l> and other link-time options

=item *

C<DEFINE> - C<-D> options, if any.

=back

The possible values for the hash supplied to the probe() method are:

=over

=item *

C<pkg> - an array of F<pkg-config> names to probe for.  If the
F<pkg-config> checks pass, C<inccheck> and C<libcheck> aren't used.

=item *

C<inccheck> - a code reference that checks if the supplied include
directory contains the required header files.

=item *

C<libcheck> - a code reference that checks if the supplied library
directory contains the required library files.  Note: the
F<Makefile.PL> version of this was supplied all of the library file
names instead.

=item *

C<libbase> - if C<inccheck> is supplied, but C<libcheck> isn't, then a
C<libcheck> that checks for C<lib>I<libbase>I<$Config{_a}> and
C<lib>I<libbase>.I<$Config{so}> is created.  If C<libopts> isn't
supplied then that can be synthesized as C<-l>C<<I<libbase>>>.

=item *

C<libopts> - if the libraries are found via C<inccheck>/C<libcheck>,
these are the C<-l> options to supply during the link phase.

=item *

C<code> - a code reference to perform custom checks.  Returns the
probe result directly.  Can also be an array ref of functions to call.

=item *

C<testcode> - test C code that is run with Devel::CheckLib.  You also
need to set C<testcodeheaders>.

=item *

C<testcodeprologue> - C code to insert between the headers and the
main function.

=item *

C<incpath> - C<$Config{path_sep}> separated list of header file
directories to check, or a reference to an array of such.

=item *

C<libpath> - C<$Config{path_sep}> separated list of library file
directories to check, or a reference to an array of such.

=back

=cut
