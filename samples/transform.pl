#perl -w
use Imager;

# generate a colorful spiral
my $newimg = Imager::transform2({
                                 width => 160, height=>160,
                                 expr => <<EOS
dist = distance(x, y, w/2, h/2);
angle = atan2(y-h/2, x-w/2);
angle2 = (dist / 10 + angle) % ( 2 * pi );
return hsv(angle*180/pi, 1, (sin(angle2)+1)/2);
EOS
                                });
$newimg->write(file=>'transform1.ppm');
