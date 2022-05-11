#!perl
use strict;
use warnings;
use Test::More;
use ExtUtils::Manifest qw(maniread);

# collect the code
my %catches;
my $mani = maniread();
for my $file (grep m(^[^/]+\.c$), keys %$mani) {
  open my $src, "<", $file
    or die "Cannot open $file from manifest: $!";
  my $catching;
  while (<$src>) {
    if ($catching) {
      push @{$catches{$catching}}, $_;
      /^ *return/ and undef $catching;
    }
    elsif (/^\s*(\w+)\s*=\s*i_img_alloc\(\)/ ||
           /^\s*(\w+)\s*=\s*im_img_alloc\(aIMCTX\)/) {
      $catching = "$file:$.";
      $catches{$catching} = [ $_ ];
    }
  }
}

my @members = qw(vtbl xsize ysize channels ch_mask isvirtual idata bytes bits type ext_data);
my $member_qr = join '|', @members;
$member_qr = qr/$member_qr/;

for my $collect (sort keys %catches) {
  note $collect;
  my @code = @{$catches{$collect}};

  # check each member is initialized
  my $name;
  my $nameqr;
  my %members_seen;
  my $did_tags;
  my $did_init;
  my $did_memset;
  for (@code) {
    if (/^\s*(\w+)\s*=\s*im?_img_alloc\(/ ||
        /^\s*(\w+)\s*=\s*im_img_alloc\(aIMCTX\)/) {
      $name = $1;
      $nameqr = qr/\Q$name/;
    }
    elsif ($name) {
      if (/${nameqr}->($member_qr)\s*=\s*(\S[^;]*)/) {
        $members_seen{$1} = $2;
      }
      elsif (/\bim_img_init\(aIMCTX,\s*$nameqr\)/ ||
             /\bi_img_init\($nameqr\)/) {
        ++$did_init;
      }
      elsif (/\bi_tags_new\(&${name}->tags\)/) {
        ++$did_tags;
      }
      elsif (/\bmemset\(${nameqr}->idata,\s*0,\s*${name}->bytes\)/) {
        ++$did_memset;
      }
    }
  }

  for my $member (@members) {
    ok(defined $members_seen{$member}, "$collect: $member initialized");
  }
  ok($did_init, "$collect: im_img_init() called");
  ok($did_tags, "$collect: im_tags_new() called");
  note $members_seen{idata};
  ok($did_memset || ($members_seen{idata} && $members_seen{idata} eq "NULL"),
     "$collect: idata memory cleared (if needed)");
}

done_testing();
