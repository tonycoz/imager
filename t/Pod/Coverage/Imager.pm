package Pod::Coverage::Imager;
use strict;
use base 'Pod::Coverage';

sub _get_pods {
  my $self = shift;

  my $package = $self->{package};

  #print "getting pod location for '$package'\n" if TRACE_ALL;
  $self->{pod_from} ||= pod_where( { -inc => 1 }, $package );

  my $pod_from = $self->{pod_from};
  $pod_from = [ $pod_from ] unless ref $pod_from;
  unless ($pod_from) {
    $self->{why_unrated} = "couldn't find pod";
    return;
  }
  
  #print "parsing '$pod_from'\n" if TRACE_ALL;
  my $pod = Pod::Coverage::Extractor->new;
  for my $pod_file (@$pod_from) {
    $pod->parse_from_file( $pod_file, '/dev/null' );
  }
  
  return $pod->{identifiers} || [];
}

1;
