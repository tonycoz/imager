#!perl -w
use strict;
use lib 't';
use Test::More;
use ExtUtils::Manifest qw(maniread);
eval "use Test::Pod::Coverage 1.08;";
# 1.08 required for coverage_class support
plan skip_all => "Test::Pod::Coverage 1.08 required for POD coverage" if $@;

# scan for a list of files to get Imager method documentation from
my $manifest = maniread();
my @pods = ( 'Imager.pm', grep /\.pod$/, keys %$manifest );

my @private = 
  ( 
   '^io?_',
   '^DSO_',
   '^Inline$',
   '^yatf$',
   '^m_init_log$',
   '^malloc_state$',
   '^init_log$',
   '^polybezier$', # not ready for public consumption
  );
my @trustme = ( '^open$',  );

plan tests => 11;

{
  pod_coverage_ok('Imager', { also_private => \@private,
			      pod_from => \@pods,
			      trustme => \@trustme,
			      coverage_class => 'Pod::Coverage::Imager' });
  pod_coverage_ok('Imager::Font');
  my @color_private = ( '^i_', '_internal$' );
  pod_coverage_ok('Imager::Color', 
		  { also_private => \@color_private });
  pod_coverage_ok('Imager::Color::Float', 
		  { also_private => \@color_private });
  pod_coverage_ok('Imager::Color::Table');
  pod_coverage_ok('Imager::ExtUtils');
  pod_coverage_ok('Imager::Expr');
  my $trust_parents = { coverage_class => 'Pod::Coverage::CountParents' };
  pod_coverage_ok('Imager::Expr::Assem', $trust_parents);
  pod_coverage_ok('Imager::Fill');
  pod_coverage_ok('Imager::Font::BBox');
  pod_coverage_ok('Imager::Font::Wrap');
}

