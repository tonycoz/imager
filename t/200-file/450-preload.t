#!perl -w
use strict;
use Imager;
use Test::More tests => 2;

$@ = "Hello";
ok(Imager->preload, "preload doesn't die");
is($@, "Hello", "check \$@ was preserved");
