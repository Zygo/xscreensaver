#!/usr/bin/perl -T -w
# xlocksat
require "ctime.pl";

#local($hour) = `/bin/date "+%H:"`;
local($hour) = (localtime)[2];
local($saturation) = (12 - abs($hour - 12)) / 12 ;
printf("%.2f\n", $saturation);
