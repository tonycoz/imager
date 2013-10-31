#!perl -w
use strict;
use Test::More;
$ENV{AUTOMATED_TESTING} || $ENV{IMAGER_AUTHOR_TESTING}
  or plan skip_all => "POD only tested under automated or author testing";
BEGIN {
  eval 'use Pod::Parser 1.50;';
  plan skip_all => "Pod::Parser 1.50 required for podlinkcheck" if $@;
}
use File::Spec::Functions qw(rel2abs abs2rel splitdir);
use ExtUtils::Manifest qw(maniread);

# this test is intended to catch errors like in
# https://rt.cpan.org/Ticket/Display.html?id=85413

my @pod; # files with pod

my $base = rel2abs(".");

my $manifest = maniread();

my @files = sort grep /\.(pod|pm)$/ && !/^inc/, keys %$manifest;

my %item_in;

for my $file (@files) {
  my $parser = PodPreparse->new;

  $parser->parse_from_file($file);
  if ($parser->{is_pod}) {
    push @pod, $file;
  }
}

plan tests =>  3 * scalar(@pod);

my @req_head1s = qw(NAME DESCRIPTION AUTHOR);

for my $file (@pod) {
  my $parser = PodStructCheck->new;
  my $relfile = abs2rel($file, $base);
  $parser->{bad_quotes} = [];
  $parser->{dup_words} = [];
  $parser->parse_from_file($file);

  my @missing;
  for my $head (@req_head1s) {
    push @missing, $head unless $parser->{head1s}{$head};
  }

  unless (ok(!@missing, "$relfile: check missing headers")) {
    diag "$relfile: missing head1s @missing\n";
  }
  unless (ok(!@{$parser->{bad_quotes}}, "$relfile: check for bad quotes")) {
    diag "$relfile:$_->[1]: bad quote in: $_->[0]"
      for @{$parser->{bad_quotes}};
  }
  unless (ok(!@{$parser->{dup_words}}, "$relfile: check for duplicate words")) {
    diag "$relfile:$_->[1]: dup word '$_->[0]' in: $_->[2]"
      for @{$parser->{dup_words}};
  }
}

package PodPreparse;
BEGIN { our @ISA = qw(Pod::Parser); }

sub command {
  my ($self, $cmd, $para) = @_;

  $self->{is_pod} = 1;
}

sub verbatim {}

sub textblock {}

package PodStructCheck;
BEGIN { our @ISA = qw(Pod::Parser); }

sub command {
  my ($self, $command, $paragraph, $line_num) = @_;

  if ($command eq "head1") {
    $paragraph =~ s/\s+\z//;
    $self->{head1s}{$paragraph} = 1;

    if ($paragraph =~ /\A[^']*'\z/
	|| $paragraph =~ /\A[^"]*"\z/
	|| $paragraph =~ /\A'[^']*\z/
	|| $paragraph =~ /\A"[^"]*\z/) {
      push @{$self->{bad_quotes}}, [ $paragraph, $line_num ];
    }
  }
}

sub verbatim {}

sub textblock {
  my ($self, $text, $line_num) = @_;

  if (my ($sample, $word) = $text =~ /(.{0,10}\b(\w+)\s+\2\b.{0,10})/s) {
    # avoid catching "C C<something to do with C>"
    if ($word ne "C") {
      push @{$self->{dup_words}}, [ $word, $line_num, $sample ];
    }
  }
}

sub sequence {
}


