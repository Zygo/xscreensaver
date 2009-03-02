#!/usr/bin/perl
# xarand
    open(S,"/usr/games/fortune|wc|");
    @numbers = split(" ",<S>);
    print int $numbers[2]/3;

