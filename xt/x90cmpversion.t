#!perl -w
use strict;
use Test::More;
use ExtUtils::MakeMaker;
use ExtUtils::Manifest 'maniread';
use File::Spec::Functions qw(devnull);

my $last_tag = `git describe --abbrev=0`;
chomp $last_tag;

$last_tag
  or plan skip_all => "Only usable in a git checkout";

my $mani = maniread();

my @subdirs = qw(PNG TIFF GIF JPEG W32 T1 FT2 ICO SGI Mandelbrot CountColor DynTest);

my $subdir_re = "^(?:" . join("|", @subdirs) . ")/";

my @pm_files = sort
  grep /\.pm$/ && !/$subdir_re/ && !/^t\// && $_ ne 'Imager.pm', keys %$mani;

plan tests => scalar(@subdirs) + scalar(@pm_files);

for my $dir (@subdirs) {
  my @changes = `git log --abbrev --oneline $last_tag..HEAD $dir`;
  my @more_changes = `git status --porcelain $dir`;
 SKIP:
  {
    @changes || @more_changes
      or skip "No changes for $dir", 1;
    my $vfile = "$dir/$dir.pm";
    my $current = eval { MM->parse_version($vfile) };
    my $last_rel_content = get_file_from_git($vfile, $last_tag);
    my $last = eval { MM->parse_version(\$last_rel_content) };
    unless (isnt($current, $last, "$dir updated, $vfile version bump")) {
      diag(@changes, @more_changes);
    }
  }
}

for my $file (@pm_files) {
  my @changes = `git log --abbrev --oneline $last_tag..HEAD $file`;
  my @more_changes = `git status --porcelain $file`;
 SKIP:
  {
    @changes || @more_changes
      or skip "No changes for $file", 1;
    my $current = eval { MM->parse_version($file) };
    my $last_rel_content = get_file_from_git($file, $last_tag);
    my $last = eval { MM->parse_version(\$last_rel_content) };
    unless (isnt($current, $last, "$file updated, version bump")) {
      diag(@changes, @more_changes);
    }
  }
}

sub get_file_from_git {
    my ($file, $tag) = @_;
    my $null = devnull();
    local $/;
    return scalar `git --no-pager show $tag:$file 2>$null`;
}
