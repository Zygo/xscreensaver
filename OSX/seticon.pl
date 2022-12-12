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

# Change the Finder icon of a file, folder or bundle.
# This is a replacement for seticon from http://osxutils.sourceforge.net/.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.10 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;


# Anything placed on this list gets unconditionally deleted when this
# script exits, even if abnormally.
#
my %rm_f;
END { rmf(); }

sub rmf() {
  foreach my $f (sort keys %rm_f) {
    if (-e $f) {
      print STDERR "$progname: rm $f\n" if ($verbose > 1);
      unlink $f;
    }
  }
  %rm_f = ();
}

sub signal_cleanup($) {
  my ($s) = @_;
  print STDERR "$progname: SIG$s\n" if ($verbose > 1);
  rmf();
  # Propagate the signal and die. This does not cause END to run.
  $SIG{$s} = 'DEFAULT';
  kill ($s, $$);
}

$SIG{TERM} = \&signal_cleanup;  # kill
$SIG{INT}  = \&signal_cleanup;  # shell ^C
$SIG{QUIT} = \&signal_cleanup;  # shell ^|
$SIG{KILL} = \&signal_cleanup;  # nope
$SIG{ABRT} = \&signal_cleanup;
$SIG{HUP}  = \&signal_cleanup;

# Add the file to the rm_f list, since you can't export hash variables.
sub rm_atexit($) {
  my ($file) = @_;
  $rm_f{$file} = 1;
}


# Like system but handles error conditions.
#
sub safe_system(@) {
  my (@cmd) = @_;
  print STDERR "$progname: exec: " . join(' ', @cmd) . "\n" if ($verbose);
  system @cmd;
  my $exit_value  = $? >> 8;
  my $signal_num  = $? & 127;
  my $dumped_core = $? & 128;
  error ("$cmd[0]: core dumped!") if ($dumped_core);
  error ("$cmd[0]: signal $signal_num!") if ($signal_num);
  return $exit_value;
}


sub set_icon($$) {
  my ($icon, $target) = @_;

  error ("no such file: $icon") unless (-f $icon);
  error ("no such file or directory: $target") unless (-e $target);
  error ("icon must be an .icns file") unless ($icon =~ m/\.icns$/si);

  my $otarget = $target;
  if (-d $target) {
    $target =~ s@/+$@@s;
    $target .= "/Icon\r";
  }

  # The Rez language is documented in "Building and Managing Programs in MPW,
  # Second Edition", Appendix C.  Here are some unarchived 404 URLs!
  # ftp://ftp.apple.com/developer/Tool_Chest/Core_Mac_OS_Tools/MPW_etc./Documentation/MPW_Reference/Building_Progs_In_MPW.sit.hqx
  # http://www.powerpc.hu/manila/static/home/Apple/developer/Tool_Chest/Core_Mac_OS_Tools/MPW_etc./Documentation/MPW_Reference/Building_Progs_In_MPW.sit.hqx

  # Rez can't read stdin, and input files must be in the current directory.

  my $n = rand() * 0xFFFFFFFF;
  my $rsrc_tmp = sprintf("rez_%08X.rsrc", $n);
  my $icon_tmp = sprintf("rez_%08X.icns", $n);
  rm_atexit ($rsrc_tmp);
  rm_atexit ($icon_tmp);
  unlink ($rsrc_tmp, $icon_tmp);

  safe_system ("cp", "-p", $icon, $icon_tmp);

  my $cmd = "Read 'icns' (kCustomIconResource) \"$icon_tmp\";\n";
  open (my $out, '>:raw', $rsrc_tmp) || error ("$rsrc_tmp: $!");
  print $out $cmd;
  close $out;
  print STDERR "$progname: $rsrc_tmp: $cmd" if ($verbose);

  safe_system ('Rez',

               '-isysroot',     # Makes it undrstand kCustomIconResource
               '/Applications/Xcode.app/Contents/Developer/Platforms' .
               '/MacOSX.platform/Developer/SDKs/MacOSX.sdk',
               'CoreServices.r',

               '-a', $rsrc_tmp,
               '-o', $target);

  # Have to also inform Finder that the icon is there, with the
  # com.apple.FinderInfo xattr (a FolderInfo struct).
  safe_system ('SetFile', '-a', 'C', $target);
  safe_system ('SetFile', '-a', 'C', $otarget) if ($target ne $otarget);
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print "Usage: $progname -d icon.icns [ files-or-folders ... ]\n";
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
  error ("no icon") unless defined($src);
  error ("no files") unless @dst;
  foreach my $f (@dst) {
    set_icon ($src, $f);
  }
}

main();
exit 0;
