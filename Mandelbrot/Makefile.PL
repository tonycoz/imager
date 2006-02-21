use ExtUtils::MakeMaker;

my %opts = 
  (
   NAME => 'Imager::Filter::Mandelbrot',
   VERSION_FROM => 'Mandelbrot.pm',
   OBJECT => 'Mandelbrot.o mandel.o',
   INC => '-I..'
  );
if ($ExtUtils::MakeMaker::VERSION > 6.06) {
  $opts{AUTHOR} = 'Tony Cook <tony@imager.perl.org>';
  $opts{ABSTRACT} = 'Mandelbrot Imager filter extension';
}

WriteMakefile(%opts);


