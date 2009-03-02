#!/usr/bin/perl
# This is used if the RANDIMAGEFILE switch does not work so instead of:
# xlock -imagefile [ DIRECTORY | FILE ] [other xlock options]
# try
# randimagefile.pl [DIRECTORY | FILE] [other xlock options]
# By the way the typical other xlock options that would be used here are
# -mode [image | puzzle] -install

local($file) = shift(@ARGV);
if ($file eq "") {
    print "\nUsage: $0 [DIRECTORY | FILE] [other xlock options]\n";
    print "\tother xlock options typically are: -mode [image | puzzle] -install\n";
    exit 1;
}

getrandfile();
#printf "xlock -imagefile $file @ARGV\n";
system "xlock -imagefile $file @ARGV";

sub getrandfile {
  if (-d $file) { # OK so its not really a file :)
    opendir(DIR, $file) || die "Can not open $file";
    local(@filenames) = grep(!/^\.\.?$/, readdir(DIR)); # ls but avoid . && ..
    closedir(DIR);
    srand(time); # use time|$$ if security involved
    $file = splice(@filenames, rand @filenames, 1); # pick one
  }
}
