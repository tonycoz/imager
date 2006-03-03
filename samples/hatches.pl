#!perl -w
use strict;
use Imager ':handy'; # handy functions like NC
use Imager::Fill;
use HTML::Entities;

if (!-d 'hatches') {
  mkdir 'hatches'
    or die "hatches directory does not exist and could not be created: $!";
}

open HTML, "> hatches.html"
  or die "Cannot create hatches.html: $!";
print HTML <<EOS;
<HTML><HEAD><TITLE>Imager - Hatched Fills</TITLE></HEAD><BODY BGCOLOR="FFFFFF">

<CENTER><FONT FACE="Helvetica, Arial" SIZE="6" COLOR="CC0000"><B>
Hatched Fills
</FONT></B></CENTER>
<HR WIDTH="65%" NOSHADE>
<TABLE><TR><TD WIDTH="70%">

<TABLE>
<TR><TH>Filled area</TH><TH>Close-up</TH><TH>Name</TH></TR>
EOS

my $red = NC(255, 0, 0);
my $yellow = NC(255, 255, 0);

# sort of a spiral
my $custom = [ 0xFF, 0x01, 0x7D, 0x45, 0x5D, 0x41, 0x7F, 0x00 ];

for my $hatch (Imager::Fill->hatches, $custom) {
  my $area = Imager->new(xsize=>100, ysize=>100);
  $area->box(xmax=>50, fill => { hatch => $hatch });
  $area->box(xmin=>50, 
             fill => { hatch => $hatch,
                       fg=>$red,
                       bg=>$yellow });
  my $name = ref($hatch) ? "custom" : $hatch;

  $area->write(file=>"hatches/area_$name.png")
    or die "Cannot save hatches/area_$name.png: ",$area->errstr;

  my $subset = $area->crop(width=>20, height=>20);
  # we use the HTML to zoom up
  $subset->write(file=>"hatches/zoom_$name.png")
    or die "Cannot save hatches/zoom_$name.png: ",$subset->errstr;

  print HTML <<EOS;
<TR>
  <TD><IMG SRC="hatches/area_$name.png" WIDTH="100" HEIGHT="100" BORDER=1></TD>
  <TD><IMG SRC="hatches/zoom_$name.png" WIDTH="100" HEIGHT="100" BORDER=1></TD>
  <TD>$name</TD>
</TR>
EOS
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
