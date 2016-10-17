#!/usr/bin/perl -w
# Copyright © 2015-2016 Dave Odell <dmo2118@gmail.com>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or
# implied warranty.

# This is a replacement for seticon from http://osxutils.sourceforge.net/.

require 5;
use diagnostics;
use strict;
#use IPC::Open2;
use File::Temp;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.5 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;

sub set_icon ($$) {
  my ($icon, $target) = @_;
  my $target_res = $target;

  if (-d $target) {
    $target_res = $target_res . "/Icon\r";
  }

  # Rez hates absolute paths, apparently.
  if ($icon =~ m@^/@s) {
    my $cwd = `pwd`;
    chomp $cwd;
    $icon =~ s@^\Q$cwd/@@s;
  }

  # The Rez language is documented in "Building and Managing Programs in MPW,
  # Second Edition". No longer available on Apple's servers, it can now be
  # found at:
  # http://www.powerpc.hu/manila/static/home/Apple/developer/Tool_Chest/Core_Mac_OS_Tools/MPW_etc./Documentation/MPW_Reference/Building_Progs_In_MPW.sit.hqx

  my $pgm = "Read 'icns' (kCustomIconResource) \"$icon\";\n";

  # Rez can read from stdin, but only if it is a file handle, not if it
  # is a pipe (OS X 10.9, Xcode 5; OSX 10.11, Xcode 6).

  my ($rez_fh, $rez_filename) = File::Temp::tempfile(DIR => '.', UNLINK => 1);
  print $rez_fh $pgm;
  close $rez_fh;

  my @cmd = ('Rez',
             'CoreServices.r',
             $rez_filename,
             '-o', $target_res);

  print STDERR "$progname: exec: " . join(' ', @cmd) . "\n$pgm\n"
    if ($verbose);

#  my ($in, $out);
#  my $pid = open2 ($out, $in, @cmd);
#  print $in $pgm;
#  close ($in);
#  waitpid ($pid, 0);

  system (@cmd);

  my $exit  = $? >> 8;
  exit ($exit) if $exit;

  # Have to also inform Finder that the icon is there, with the
  # com.apple.FinderInfo xattr (a FolderInfo struct).
  @cmd = ('SetFile', '-a', 'C', $target);
  system (@cmd);
  $exit  = $? >> 8;
  exit ($exit) if $exit;
}

sub usage() {
  print "Usage: $progname -d source [file...]\n";
  exit 1;
}

sub main() {
  my ($src, @dst);
  while ($#ARGV >= 0) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/s)      { $verbose++; }
    elsif (m/^-v+$/s)          { $verbose += length($_)-1; }
    elsif (m/^-d$/s)           { $src = shift @ARGV; }
    elsif (m/^-/s)             { usage(); }
    else { push @dst, $_; }
  }
  error ("no source") unless defined($src);
  error ("no files") unless @dst;
  foreach my $f (@dst) {
    set_icon ($src, $f);
  }
}

main();
exit 0;
