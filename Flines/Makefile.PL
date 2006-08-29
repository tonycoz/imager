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
my $MM_ver = eval $ExtUtils::MakeMaker::VERSION;
if ($MM_ver > 6.06) {
  $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>';
  $opts{ABSTRACT} = 'Flines Imager filter extension';
}

WriteMakefile(%opts);


