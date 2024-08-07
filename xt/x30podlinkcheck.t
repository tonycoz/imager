#!perl -w
use strict;
use Test::More;
BEGIN {
  eval 'use Pod::Parser 1.50;';
  plan skip_all => "Pod::Parser 1.50 required for podlinkcheck" if $@;
}
use File::Find;
use File::Spec::Functions qw(rel2abs abs2rel splitdir);

# external stuff we refer to
my @known =
  qw(perl Affix::Infix2Postfix Parse::RecDescent GD Image::Magick Graphics::Magick CGI Image::ExifTool
     XSLoader DynaLoader Prima::Image IPA PDL Imager::File::APNG ExtUtils::MakeMaker);

# also known since we supply them, but we don't always install them
push @known, qw(Imager::Font::FT2 Imager::Font::W32 Imager::Font::T1
   Imager::File::JPEG Imager::File::GIF Imager::File::PNG Imager::File::TIFF);

my @pod; # files with pod

my $base = rel2abs("blib/lib");

my @files;
find(sub {
       -f && /\.(pod|pm)$/
	 and push @files, $File::Find::name;
     }, $base);

my %targets = map { $_ => {} } @known;
my %item_in;

for my $file (@files) {
  my $parser = PodPreparse->new;

  my $link = abs2rel($file, $base);
  $link =~ s/\.(pod|pm|pl|PL)$//;
  $link = join("::", splitdir($link));

  $parser->{'targets'} = \%targets;
  $parser->{'link'} = $link;
  $parser->{'file'} = $file;
  $parser->{item_in} = \%item_in;
  $parser->parse_from_file($file);
  if ($targets{$link}) {
    push @pod, $file;
  }
}

plan tests => scalar(@pod);

for my $file (@pod) {
  my $parser = PodLinkCheck->new;
  $parser->{"targets"} = \%targets;
  my $relfile = abs2rel($file, $base);
  (my $link = $relfile) =~ s/\.(pod|pm|pl|PL)$//;
  $link = join("::", splitdir($link));
  $parser->{"file"} = $relfile;
  $parser->{"link"} = $link;
  my @errors;
  $parser->{"errors"} = \@errors;
  $parser->{item_in} = \%item_in;
  $parser->parse_from_file($file);

  unless (ok(!@errors, "check links in $relfile")) {
    print STDERR "# $relfile:$_\n" for @errors;
  }
}

package PodPreparse;
BEGIN { our @ISA = qw(Pod::Parser); }

sub command {
  my ($self, $cmd, $para) = @_;

  my $targets = $self->{"targets"};
  my $link = $self->{"link"};
  $targets->{$link} ||= {};

  if ($cmd =~ /^(head[1-5]|item)/) {
    $para =~ s/X<.*?>//g;
    $para =~ s/\s+$//;
    $targets->{$link}{$para} = 1;
    push @{$self->{item_in}{$para}}, $link;
  }
}

sub verbatim {}

sub textblock {}

package PodLinkCheck;
BEGIN { our @ISA = qw(Pod::Parser); }

sub command {}

sub verbatim {}

sub textblock {
  my ($self, $para, $line_num) = @_;

  $self->parse_text
    (
     { -expand_seq => "sequence" },
     $para, $line_num,
    );
}

sub sequence {
  my ($self, $seq) = @_;

  if ($seq->cmd_name eq "L") {
    my $raw = $seq->raw_text;
    my $base_link = $seq->parse_tree->raw_text;
    (my $link = $base_link) =~ s/.*\|//s;
    $link =~ /^(https?|ftp|mailto):/
      and return '';
    my ($pod, $part) = split m(/), $link, 2;
    $pod ||= $self->{link};
    if ($part) {
      $part =~ s/^\"//;
      $part =~ s/"$//;
    }
    my $targets = $self->{targets};
    my $errors = $self->{errors};
    (undef, my $line) = $seq->file_line;

    if (!$targets->{$pod}) {
      push @$errors, "$line: No $pod found ($raw)";
    }
    elsif ($part && !$targets{$pod}{$part}) {
      push @$errors, "$line: No item/section '$part' found in $pod ($raw)";
      if ($self->{item_in}{$part}) {
	push @$errors, "  $part can be found in:";
	push @$errors, map "     $_", @{$self->{item_in}{$part}};
      }
    }
  }

  return $seq->raw_text;
}

