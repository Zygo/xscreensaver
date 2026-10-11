#!/usr/bin/perl -w
# Copyright © 2025-2026 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# "Compile" a set of shadertoy GLSL scripts into a standalone XScreenSaver
# module that runs "xshadertoy" with those scripts.
#
# Created: 26-Dec-2025.

require 5;
use diagnostics;
use strict;
use utf8;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.03 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 1;

sub compile($@) {
  my ($outfile, @files) = @_;

  my %variants;

  foreach my $f (@files) {
    error ("$f: not a GLSL file") unless ($f =~ m/\.glsl$/s);
    my ($variant, $idx);
    if ($f =~ m@^.*[^-](\d)-(\d|c(?:ommon)?)\.glsl$@s) {	# nameI-J.glsl
      ($variant, $idx) = ($1, $2);
    } elsif ($f =~ m@^.*[^-](\d)\.glsl$@s) {			# nameI.glsl
      ($variant, $idx) = ($1, 0);
    } elsif ($f =~ m@^.*[^-\d]-(\d|c(?:ommon)?)\.glsl$@s) {	# name-J.glsl
      ($variant, $idx) = (0, $1);
    } elsif ($f =~ m@^.*[^-\d]\.glsl$@s) {			# name.glsl
      ($variant, $idx) = (0, 0);
    } else {
      error ("unparsable: $f");
    }

    error ("max of 5 programs: $f") if ($idx =~ m/^\d+$/ && $idx > 5);

    my $set = $variants{$variant} || {};
    error ("$variant-$idx already exists") if ($set->{$idx});
    $set->{$idx} = $f;
    $variants{$variant} = $set;
  }

  my $out = '';
  my @switches = ();

  my $comment = undef;
  my $nvariants = scalar (keys %variants);
  my $total = 0;
  foreach my $variant (sort keys %variants) {
    my $set = $variants{$variant};
    my $nfiles = scalar (keys %$set);
    foreach my $idx (sort keys %$set) {
      my $f = $set->{$idx};
      my $common = ($set->{'common'} || $set->{'c'});
      my $commonp = ($common && $f eq $common);
      my $body = '';
      open (my $in, '<:utf8', $f) || error ("$f: $!");
      local $/ = undef;         # read entire file
      while (<$in>) { $body .= $_; }

      # Extract the first comment from the first file.

      ($comment) = ($body =~ m@^((\s*//[^\n]*\n|\s*/\*.*?\*/[ \t]*)+)@si)
        unless ($comment);

      # Omit the rest of the comments.

      $body =~ s@//[^\n]*@@gs;	# It seems that // is parsed before /*
      $body =~ s@/\*.*?\*/@@gs;
      $body =~ s/^[ \t]+|[ \t]+$//gm;
      $body =~ s/\n\n+/\n/gs;
      $body =~ s/^\s+|\s+$//gs;

      $out .= "\n.\n" unless ($total == 0);	# EOF marker between files.

      if ($nvariants > 1 || $nfiles > 1) {
        my $name = (($nvariants > 1 ? "$variant " : "") .
                    ($commonp ? 'Common' :
                     $idx == $nfiles-1
                     ? 'Image'
                     : 'Buffer' . chr(ord('A') + $idx)));
        $out .= "// $name\n\n";
      }

      my $switch = (" --program" .
                    ($nvariants > 1 ? "$variant-$idx" : "$idx") .
                    " -");
      push @switches, $switch;

      $body =~ s/^/\t/gm;
      $out .= $body . "\n";
      $total++;
    }
  }

  $out = "$comment\n\n$out" if ($comment);

  my $title = $outfile;
  $title =~ s@^.*/@@gs;

  my $year = (localtime)[5] + 1900;
  my $head = '#!/bin/bash
# XScreenSaver, Copyright © ' . $year . ' Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.

PATH="$PATH":"$(dirname "$0")"
exec -a "' . $title . '" \
xshadertoy "$@" \
';

  $head .= join (" \\\n", @switches) . " \\\n";

  $head .= "<< \"_XSCREENSAVER_EOF_\"\n\n";
  $out = "$head$out\n_XSCREENSAVER_EOF_\n";
 
  open (my $of, '>:utf8', $outfile) || error ("$outfile: $!");
  print $of $out;
  chmod (oct("0755"), $of);
  close $of;
  print STDERR "$progname: wrote $outfile\n" if ($verbose);
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose] [--quiet] --name NAME [ GLSL_FILES ...]\n";
  exit 1;
}

sub main() {
  my $name;
  my @files = ();
  while (@ARGV) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/s) { $verbose++; }
    elsif (m/^-v+$/s) { $verbose += length($_)-1; }
    elsif (m/^--?q(uiet)?$/s) { $verbose = 0; }
    elsif (m/^--?name$/s) { $name = shift @ARGV; }
    elsif (m/^-./s) { usage; }
    else { push @files, $_; }
  }

  usage unless ($name);
  usage unless (@files);
  compile ($name, @files);
}

main();
exit 0;
