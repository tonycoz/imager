BEGIN { $| = 1; print "1..7\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager::Expr;

$loaded = 1;
print "ok 1\n";

# only test this if Parse::RecDescent was loaded successfully
eval "use Parse::RecDescent";
if (!$@) {
  my $opts = {expr=>'z=0.8;return hsv(x/w*360,y/h,z)', variables=>[ qw(x y) ], constants=>{h=>100,w=>100}};
  my $expr = Imager::Expr->new($opts);
  if ($expr) {
    print "ok 2\n";
    my $code = $expr->dumpcode();
    my @code = split /\n/,$code;
    #print $code;
    print $code[-1] =~ /:\s+ret/ ? "ok 3\n" : "not ok 3\n";
    print grep(/:\s+mult.*360/, @code) ? "ok 4\n" : "not ok 4\n";
    # strength reduction converts these to mults
    #print grep(/:\s+div.*x/, @code) ? "ok 5\n" : "not ok 5\n";
    #print grep(/:\s+div.*y/, @code) ? "ok 6\n" : "not ok 6\n";
    print grep(/:\s+mult.*x/, @code) ? "ok 5\n" : "not ok 5\n";
    print grep(/:\s+mult.*y/, @code) ? "ok 6\n" : "not ok 6\n";
    print grep(/:\s+hsv.*0\.8/, @code) ? "ok 7\n" : "not ok 7\n";
  }
  else {
    print "not ok 2 # ",Imager::Expr::error(),"\n";
    print "not ok 3 # skipped\n";
    print "not ok 4 # skipped\n";
    print "not ok 5 # skipped\n";
    print "not ok 6 # skipped\n";
    print "not ok 7 # skipped\n";
  }
}
else {
  print "ok 2 # skipped - no Parse::RecDescent\n";
  print "ok 3 # skipped - no Parse::RecDescent\n";
  print "ok 4 # skipped - no Parse::RecDescent\n";
  print "ok 5 # skipped - no Parse::RecDescent\n";
  print "ok 6 # skipped - no Parse::RecDescent\n";
  print "ok 7 # skipped - no Parse::RecDescent\n";
}
