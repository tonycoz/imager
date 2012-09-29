#!perl -w
use strict;
use Test::More;
use ExtUtils::MakeMaker;
use File::Spec::Functions qw(devnull);

my $last_tag = `git describe --abbrev=0`;
chomp $last_tag;

$last_tag
  or plan skip_all => "Only usable in a git checkout";

my @subdirs = qw(PNG TIFF GIF JPEG W32 T1 FT2);

plan tests => scalar(@subdirs);

for my $dir (@subdirs) {
  my @changes = `git log --abbrev --oneline $last_tag..HEAD $dir`;
 SKIP:
  {
    @changes or skip "No changes for $dir", 1;
    my $vfile = "$dir/$dir.pm";
    my $current = eval { MM->parse_version($vfile) };
    my $last_rel_content = get_file_from_git($vfile, $last_tag);
    my $last = eval { MM->parse_version(\$last_rel_content) };
    isnt($current, $last, "$dir updated, $vfile version bump");
  }
}

sub get_file_from_git {
    my ($file, $tag) = @_;
    my $null = devnull();
    local $/;
    return scalar `git --no-pager show $tag:$file 2>$null`;
}
