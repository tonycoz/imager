# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..2\n"; }
END {print "not ok 1\n" unless $loaded;}
use Imager;
#use Data::Dumper;
$loaded = 1;
print "ok 1\n";

init_log("testout/t00basic.log",1);

#list_formats();

#%hsh=%Imager::formats;
#print Dumper(\%hsh);

i_has_format("jpeg") && print "# has jpeg\n";
i_has_format("png") && print "# has png\n";

print "ok 2\n";

malloc_state();
