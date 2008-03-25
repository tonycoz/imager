#!perl -w
use blib;
use strict;
use Imager;
use Getopt::Long;
use Time::HiRes qw(time);

my $count = 1000;
GetOptions("c=i"=>\$count)
  or usage();

my @files = @ARGV;
unless (@files) {
  @files = glob('testimg/*.gif'), glob('testout/*');
}

@files = grep -f, @files;
Imager->set_file_limits(bytes => 20_000_000);
++$|;
for my $i (1 .. $count) {
  my $filename = $files[rand @files];
  open FILE, "< $filename" or die "Cannot read $filename: $!\n";
  binmode FILE;
  my $data = do { local $/; <FILE> };
  close FILE;

  print ">> $filename - length ", length $data, "\n";
  if (rand() < 0.2) {
    # random truncation
    my $new_length = int(1 + rand(length($data)-2));
    substr($data, $new_length) = '';
    print "  trunc($new_length)\n";
  }
  # random damage
  for (0..int(rand 5)) {
    my $offset = int(rand(length($data)));
    my $len = int(1+rand(5));
    if ($offset + $len > length $data) {
      $len = length($data) - $offset;
    }
    my $ins = join '', map chr(rand(256)), 1..$len;
    print "  replace $offset/$len: ", unpack("H*", $ins), "\n";
    substr($data, $offset, $len, $ins);
  }
  my $start = time;
  my $im = Imager->new;
  my $result = $im->read(data => $data);
  my $dur = time() - $start;
  if ($dur > 1.0) {
    print "***Took too long to load\n";
  }
  printf "   Took %f seconds\n", time() - $start;
  if ($result) {
    print "<< Success\n";
  }
  else {
    print "<< Failure: ", $im->errstr, "\n";
  }
}

sub usage {
  print <<EOS;
perl $0 [-c count] [files]
EOS
  exit;
}
