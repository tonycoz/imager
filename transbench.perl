#!perl -w
use Imager;
use Benchmark;

$img = Imager->new();
$img->open(file=>'testimg/penguin-base.jpg', type=>'jpeg')
	|| die "Cannot open penguin-base.jpg";

timethese(-10,
	{ old=><<'EOS',
$im2 = $img->transform(xexpr=>'x', yexpr=>'y+10*sin((x+y)/10)');
EOS
	new=><<'EOS'
$im2 = Imager::transform2({rpnexpr=>'x y 10 x y + 10 / sin * + getp1'}, $img);
EOS
}
);
timethese(-10,
	{ old=><<'EOS',
$im2 = $img->transform(xexpr=>'x', yexpr=>'y+(x+y)/10');
EOS
	new=><<'EOS'
$im2 = Imager::transform2({rpnexpr=>'x y x y + 10 / + getp1'}, $img);
EOS
}
);

