BEGIN { $| = 1; print "1..6\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager::Expr;

$loaded = 1;
print "ok 1\n";

#$Imager::DEBUG=1;

my $expr = Imager::Expr->new({rpnexpr=>'x two * y one + getp1', variables=>[ qw(x y) ], constants=>{one=>1, two=>2}});
if ($expr) {
  print "ok 2\n";

  # perform some basic validation on the code
  my $code = $expr->dumpcode();
  my @code = split /\n/, $code;
  print $code[-1] =~ /:\s+ret/ ? "ok 3\n" : "not ok 3\n";
  print grep(/:\s+mult.*x/, @code) ? "ok 4\n" : "not ok 4\n";
  print grep(/:\s+add.*y/, @code) ? "ok 5\n" : "not ok 5\n";
  print grep(/:\s+getp1/, @code) ? "ok 6\n" : "not ok 6\n";
}
else {
  print "not ok 2 ",Imager::Expr::error(),"\n";
  print "not ok 3 # skip\n";
  print "not ok 4 # skip\n";
  print "not ok 5 # skip\n";
  print "not ok 6 # skip\n";
}
