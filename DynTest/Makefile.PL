#!perl -w
use strict;
use ExtUtils::MakeMaker;

my %opts = 
  (
   NAME => 'Imager::Filter::DynTest',
   VERSION_FROM => 'DynTest.pm',
   OBJECT => 'DynTest.o linstretch.o',
   INC => '-I..'
  );
if ($ExtUtils::MakeMaker::VERSION > 6.06) {
  $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>';
  $opts{ABSTRACT} = 'Demo Imager filter extension';
}

WriteMakefile(%opts);


