#!perl -w
use strict;
use Imager ':handy';
use Imager::Fill;
use Imager::Fountain;
use HTML::Entities;

my $fountain = Imager::Fountain->new();
$fountain->add(end=>0.8, 
               c0=>NC(hsv=>[90, 0.5, 0.5], alpha=>64),
               c1=>NC(hsv=>[90, 1, 0.8], alpha=>192),
               color=>'hueup');
$fountain->add(start=>0.8,
               c0=>NC(hsv=>[90, 1, 0.8], alpha=>192),
               c1=>NC(255, 255, 255, 128));

unless (-d 'combines') {
  mkdir 'combines' 
    or die "combines directory does not exist and could not be created: $!";
}

open HTML, "> combines.html"
  or die "Cannot create combines.html: $!";

print HTML <<EOS;
<HTML><HEAD><TITLE>Imager - Combining Modes</TITLE></HEAD><BODY BGCOLOR="FFFFFF">

<CENTER><FONT FACE="Helvetica, Arial" SIZE="6" COLOR="CC0000"><B>
Combining Modes
</FONT></B></CENTER>
<HR WIDTH="65%" NOSHADE>
<TABLE><TR><TD WIDTH="70%">

<TABLE>
<TR><TH>Name</TH><TH>Result</TH><TH>Base</TH></TR>
EOS

# build our base image
# this is similar to the test image used for some of Imager's tests
my $green = NC(0, 255, 0);
my $lblue = NC(128, 128, 255);
my $dred = NC(128, 64, 64);
my $base = Imager->new(xsize=>150, ysize=>150);
$base->box(xmin=>70, ymin=>25, xmax=>130, ymax=>125, color=>$green, filled=>1);
$base->box(xmin=>20, ymin=>25, xmax=>80, ymax=>125, color=>$lblue, filled=>1);
$base->arc(r=>30, color=>$dred);
$base->filter(type=>'conv', coef=>[0.1, 0.2, 0.4, 0.2, 0.1]);

$base->write(file=>"combines/base.jpg", jpegquality=>100)
  or die "Cannot save combines/base.jpg: ",$base->errstr;

# we fill the top with a fountain fill, and the bottom with a solid color
my $solid_left = NC(128, 32, 32, 128);
my $solid_right = NC(128, 32, 32, 255);

for my $combine (Imager::Fill->combines) {
  my $work = $base->copy();
  $work->box(ymax=>74, 
             fill=>{ fountain=>'radial', 
                     combine=>$combine,
                     segments=>$fountain,
                     xa=>75, ya=>50, xb=>15, yb=>50,
                     repeat=>'sawtooth'
                   })
    or die $work->errstr;
  $work->box(ymin=>75, xmax=>74, 
             fill=>{ solid=>$solid_left, combine=>$combine })
    or die $work->errstr;
  $work->box(ymin=>75, xmin=>75, 
             fill=>{ solid=>$solid_right, combine=>$combine })
    or die $work->errstr;

  $work->write(file=>"combines/$combine.jpg", jpegquality=>100)
    or die "Cannot save combines/$combine.jpg: ",$work->errstr;

  print HTML <<HTML
<TR>
  <TD>$combine</TD>
  <TD><IMG SRC="combines/$combine.jpg" WIDTH="150" HEIGHT="150"></TD>
  <TD><IMG SRC="combines/base.jpg" WIDTH="150" HEIGHT="150"></TD>
</TR>
HTML
}

print HTML <<EOS;
</TABLE>

<P>The following code was used to generate this page:</p>

<PRE>
EOS

open SELF, "< $0"
  or die "Can't open myself: $!";
while (<SELF>) {
  print HTML encode_entities($_);
}
close SELF;

print HTML <<EOS;
</PRE>

<HR WIDTH="75%" NOSHADE ALIGN="LEFT">

Send errors/fixes/suggestions to: <B>tony</B>_at_<B>develop-help.com</B>

</TD></TR></TABLE>
</BODY>
</HTML>
EOS

close HTML;
