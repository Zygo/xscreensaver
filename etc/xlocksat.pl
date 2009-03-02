#!/usr/bin/perl
# xlocksat

$h = `/bin/date "+%H:"`;
$sat = (12 - abs($h - 12)) / 12 ;
printf("%.2f\n",$sat);
