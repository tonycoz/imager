#!perl -w
use strict;
use lib 't';
use Test::More;
use ExtUtils::Manifest qw(maniread);
eval "use Test::Pod::Coverage 1.08;";
# 1.08 required for coverage_class support
plan skip_all => "Test::Pod::Coverage 1.08 required for POD coverage" if $@;
plan tests => 1;

# scan for a list of files to get Imager method documentation from
my $manifest = maniread();
my @pods = ( 'Imager.pm', grep /\.pod$/, keys %$manifest );

my @private = ( '^io?_', '^DSO_', '^Inline$', '^yatf$', '^m_init_log$' );
my @trustme = ( '^errstr$', '^open$' );

TODO: {
  local $TODO = "Still working on method coverage";
  pod_coverage_ok('Imager', { also_private => \@private,
			      pod_from => \@pods,
			      trustme => \@trustme,
			      coverage_class => 'Pod::Coverage::Imager' },
		  "Imager");
		  
}

