#!perl -w
use strict;
<>; #drop the uname
my @text = <>;
$text[0] =~ s/\$VAR1/\$data/;
my $data;
eval join '', @text;
print <<EOS;
                     |       closest      |      errdiff       |
algorithm |  image   | mono | addi |webmap| mono | addi |webmap|
EOS
for my $algo (qw(hashbox sortchan linsearch rand2dist)) {
  for my $image (qw(rgbtile hsvgrad kscdisplay)) {
    printf("%-10s|%-10s|", $algo, $image);
    for my $tran (qw(closest errdiff)) {
      for my $pal (qw(mono addi webmap)) {
	printf("%6.2f|", $data->{$algo}{$image}{$tran}{$pal});
      }
    }
    print "\n";
  }
}

__END__

=head1 NAME

 benchform.perl - formats quantbench.perl results into a table.

=head1 SYNOPSIS

 perl benchform.perl quantbench.txt

=cut
