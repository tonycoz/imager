#!/usr/bin/perl 

use Imager;
print "Filter          Arguments\n";
for $filt (keys %Imager::filters) {
    @callseq=@{$Imager::filters{$filt}{'callseq'}};
    %defaults=%{$Imager::filters{$filt}{'defaults'}};
    shift(@callseq);
    @b=map { exists($defaults{$_}) ? $_.'('.$defaults{$_}.')' : $_ } @callseq;
    $str=join(" ",@b);    
    printf("%-15s %s\n",$filt,$str );
}
