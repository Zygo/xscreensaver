#!/usr/bin/perl -w
# Copyright © 2006-2012 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# Converts and installs a thumbnail image inside a .saver bundle.
#
# Created:  26-Jul-2012.

require 5;
#use diagnostics;	# Fails on some MacOS 10.5 systems
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my $version = q{ $Revision: 1.22 $ }; $version =~ s/^[^0-9]+([0-9.]+).*$/$1/;

my $verbose = 1;


sub safe_system(@) {
  my @cmd = @_;
  system (@cmd);
  my $exit_value  = $? >> 8;
  my $signal_num  = $? & 127;
  my $dumped_core = $? & 128;
  error ("$cmd[0]: core dumped!") if ($dumped_core);
  error ("$cmd[0]: signal $signal_num!") if ($signal_num);
  error ("$cmd[0]: exited with $exit_value!") if ($exit_value);
}


# Returns true if the two files differ (by running "cmp")
#
sub cmp_files($$) {
  my ($file1, $file2) = @_;

  my @cmd = ("cmp", "-s", "$file1", "$file2");
  print STDERR "$progname: executing \"" . join(" ", @cmd) . "\"\n"
    if ($verbose > 3);

  system (@cmd);
  my $exit_value  = $? >> 8;
  my $signal_num  = $? & 127;
  my $dumped_core = $? & 128;

  error ("$cmd[0]: core dumped!") if ($dumped_core);
  error ("$cmd[0]: signal $signal_num!") if ($signal_num);
  return $exit_value;
}


sub update($$) {
  my ($src_dir, $app_dir) = @_;

  # Apparently Apple wants Resources/{thumbnail.png to be 90x58,
  # and Resources/thumbnail@2x.png to be 180x116.  Let's just 
  # make the former, but make it be the latter's size.
  #
  my $size = '180x116';

  error ("$app_dir does not exist") unless (-d $app_dir);
  error ("$app_dir: no name")
    unless ($app_dir =~ m@/([^/.]+).(saver|app)/?$@x);
  my $app_name = $1;

  $app_dir =~ s@/+$@@s;
  $app_dir .= "/Contents/Resources";

  error ("$app_dir does not exist") unless (-d $app_dir);
  my $target = "$app_dir/thumbnail.png";

  $src_dir .= "/" unless ($src_dir =~ m@/$@s);
  my $src_dir2 = "${src_dir}retired/";

  $app_name =~ s/rdbomb/rd-bomb/si;   # sigh

  my $img  = $src_dir  . lc($app_name) . ".jpg";
  my $img2 = $src_dir2 . lc($app_name) . ".jpg";
  $img = $img2 if (! -f $img && -f $img2);
  error ("$img does not exist") unless (-f $img);

  my $tmp = sprintf ("%s/thumb-%08x.png",
                     ($ENV{TMPDIR} ? $ENV{TMPDIR} : "/tmp"),
                     rand(0xFFFFFFFF));
  my @cmd = ("convert",
             $img, 
             "-resize", $size . "^",
             "-gravity", "center",
             "-extent", $size,
             $tmp);

  print STDERR "$progname: exec: " . join(' ', @cmd) . "\n" 
    if ($verbose > 2);
  safe_system (@cmd);

  if (! -s $tmp) {
    unlink $tmp;
    error ("failed: " . join(" ", @cmd));
  }

  if (! cmp_files ($tmp, $target)) {
    unlink $tmp;
    print STDERR "$progname: $target: unchanged\n" if ($verbose > 1);
  } elsif (! rename ($tmp, $target)) {
    unlink $tmp;
    error ("mv $tmp $target: $!");
  } else {
    print STDERR "$progname: wrote $target\n" if ($verbose);
  }
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose] image-dir program.app ...\n";
  exit 1;
}

sub main() {

  my $src_dir;
  my @files = ();
  while ($_ = $ARGV[0]) {
    shift @ARGV;
    if    (m/^--?verbose$/s)  { $verbose++; }
    elsif (m/^-v+$/)          { $verbose += length($_)-1; }
    elsif (m/^--?q(uiet)?$/s) { $verbose = 0; }
    elsif (m/^-/s)            { usage(); }
    elsif (! $src_dir)        { $src_dir = $_; }
    else                      { push @files, $_; }
  }
  usage() unless ($src_dir && $#files >= 0);
  foreach (@files) {
    update ($src_dir, $_);
  }
}

main();
exit 0;
