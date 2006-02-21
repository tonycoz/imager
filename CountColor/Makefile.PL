use ExtUtils::MakeMaker;

my %opts = 
  (
   NAME => 'Imager::CountColor',
   VERSION_FROM => 'CountColor.pm',
   OBJECT => 'CountColor.o',
   INC => '-I..',
  );
if ($ExtUtils::MakeMaker::VERSION > 6.06) {
  $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>';
  $opts{ABSTRACT} = 'Color Count an Imager image';
}

WriteMakefile(%opts);


