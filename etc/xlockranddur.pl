#!/usr/bin/perl -w
# xarand
open(S,"/usr/games/fortune|wc|");
local(@numbers) = split(" ",<S>);
print int $numbers[2]/3;
close(S);
