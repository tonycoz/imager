#!perl -w
use strict;
use ExtUtils::MakeMaker;

my %opts = 
  (
   NAME => 'Imager::Filter::Flines',
   VERSION_FROM => 'Flines.pm',
   OBJECT => 'Flines.o',
   INC => '-I..'
  );
if ($ExtUtils::MakeMaker::VERSION > 6.06) {
  $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>';
  $opts{ABSTRACT} = 'Flines Imager filter extension';
}

WriteMakefile(%opts);


