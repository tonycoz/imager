#!perl -w
use strict;
use ExtUtils::MakeMaker;

my %opts = 
  (
   NAME => 'Imager::Font::BuiltIn',
   VERSION_FROM => 'BuiltIn.pm',
   OBJECT => 'BuiltIn.o imbif.o',
   INC => '-I..',
  );
my $MM_ver = eval $ExtUtils::MakeMaker::VERSION;
if ($MM_ver > 6.06) {
  $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>';
  $opts{ABSTRACT} = 'Built-in fonts for Imager';
}

WriteMakefile(%opts);


