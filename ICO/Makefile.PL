#!perl -w
use strict;
use ExtUtils::MakeMaker;

my %opts = 
  (
   NAME => 'Imager::File::ICO',
   VERSION_FROM => 'ICO.pm',
   OBJECT => 'ICO.o msicon.o imicon.o',
   INC => '-I..'
  );
if ($ExtUtils::MakeMaker::VERSION > 6.06) {
  $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>';
  $opts{ABSTRACT} = 'Icon Image file support';
}

WriteMakefile(%opts);


