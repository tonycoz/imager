# some versions of EU::MM have problems with recursive Makefile.PLs with
# this method defined.
undef &MY::metafile;

sub MY::metafile {
  my ($self) = @_;

  my $meta = <<YAML;
--- #YAML:1.0
name: $self->{NAME}
version: $self->{VERSION}
version_from: $self->{VERSION_FROM}
author: $self->{AUTHOR}
abstract: $self->{ABSTRACT}
installdirs: $self->{INSTALLDIRS}
YAML
  if (keys %{$Recommends{$self->{NAME}}}) {
    $meta .= "recommends:\n";
    while (my ($module, $version) = each %{$Recommends{$self->{NAME}}}) {
      $meta .= "$module: $version\n";
    }
  }
  $meta .= <<YAML;
license: perl
dynamic_config: 1
distribution_type: module
generated_by: $self->{NAME} version $self->{VERSION}
YAML
  open META, "> meta.tmp" or die "Cannot create meta.tmp: $!";
  print META $meta;
  close META;
  
  return sprintf "metafile :\n\t\$(CP) meta.tmp META.yml\n";
}

1;
