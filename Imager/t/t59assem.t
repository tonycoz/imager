BEGIN { $| = 1; print "1..6\n"; }
END { print "not ok 1\n" unless $loaded;}
use Imager::Expr::Assem;

$loaded = 1;
print "ok 1\n";

my $expr = Imager::Expr->new
  ({assem=><<EOS,
	var count:n ; var p:p
	count = 0
	p = getp1 x y
loop:
# this is just a delay
	count = add count 1
	var temp:n
	temp = lt count totalcount
	jumpnz temp loop
	ret p
EOS
    variables=>[qw(x y)],
    constants=>{totalcount=>5}
   });
if ($expr) {
  print "ok 2\n";
  my $code = $expr->dumpcode();
  my @code = split /\n/, $code;
  print $code[-1] =~ /:\s+ret/ ? "ok\n" : "not ok # missing ret\n";
  print $code[0] =~ /:\s+set/ ? "ok\n" : "not ok # missing set\n";
  print $code[1] =~ /:\s+getp1/ ? "ok\n" : "not ok # missing getp1\n";
  print $code[3] =~ /:\s+lt/ ? "ok\n" : "not ok # missing lt\n";
}
else {
  print "not ok 2 #",Imager::Expr->error(),"\n";
  for (3..6) {
    print "not ok $_\n";
  }
}
