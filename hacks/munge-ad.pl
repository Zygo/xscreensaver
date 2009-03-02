#!/usr/bin/perl -w
# Copyright © 2008 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# This updates driver/XScreenSaver.ad.in with the current list of savers.
#
# Created:  3-Aug-2008.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my $version = q{ $Revision: 1.2 $ }; $version =~ s/^[^\d]+([\d.]+).*/$1/;

my $verbose = 0;

# These are marked as disabled by default in the .ad file.
#
my %disable = ( 
   'abstractile' => 1,
   'antinspect' => 1,
   'antmaze' => 1,
   'ant' => 1,
   'carousel' => 1,
   'critical' => 1,
   'demon' => 1,
   'dnalogo' => 1,
   'glblur' => 1,
   'glforestfire' => 1,
   'glslideshow' => 1,
   'hyperball' => 1,
   'juggle' => 1,
   'laser' => 1,
   'lcdscrub' => 1,
   'lightning' => 1,
   'lisa' => 1,
   'lissie' => 1,
   'lmorph' => 1,
   'loop' => 1,
   'rotor' => 1,
   'sballs' => 1,
   'sierpinski' => 1,
   'sphere' => 1,
   'spiral' => 1,
   'thornbird' => 1,
   'vidwhacker' => 1,
   'vines' => 1,
   'webcollage' => 1,
   'worm' => 1,
  );


sub munge_ad($) {
  my ($file) = @_;
  my $body = '';
  local *IN;
  open (IN, "<$file") || error ("$file: $!");
  while (<IN>) { $body .= $_; }
  close IN;
  my $obody = $body;

  my ($top, $mid, $bot) = ($body =~ m/^(.*?\n)(\*hacks\..*?\n)(\n.*)$/s);

  my $mid2 = '';

  my %hacks;

  # Update the "*hacks.foo.name" section of the file based on the contents
  # of config/*.xml.
  #
  my $dir = $file;
  $dir =~ s@/[^/]*$@@s;
  my @counts = (0,0,0,0,0,0);
  foreach my $xml (sort (glob ("$dir/../hacks/config/*.xml"))) {
    my $b = '';
    open (IN, "<$xml") || error ("$xml: $!");
    while (<IN>) { $b .= $_; }
    close IN;
    my ($name) = ($b =~ m@<screensaver[^<>]*\b_label=\"([^<>\"]+)\">@s);
    error ("$xml: no name") unless $name;

    my $name2 = lc($name);
    $name2 =~ s/^((x|gl)?[a-z])/\U$1/s;  # what prefs.c (make_hack_name) does

    $xml =~ s@^.*/([^/]+)\.xml$@$1@s;
    if ($name ne $name2) {
      my $s = sprintf("*hacks.%s.name:", $xml);
      $mid2 .= sprintf ("%-28s%s\n", $s, $name);
      $counts[1]++;
    }

    # Grab the year.
    my ($year) =
      ($b =~ m/<_description>.*Written by.*?;\s+(19[6-9]\d|20\d\d)\b/si);
    error ("no year in $xml.xml") unless $year;
    $hacks{$xml} = $year;
    $counts[0]++;
  }

  # Splice in new names.
  $body = $top . $mid2 . $bot;


  # Replace the "programs" section.
  # Sort hacks by creation date, but put the OpenGL ones at the end.
  #
  my $segregate_p = 0;  # whether to put the GL hacks at the end.
  my $xhacks = '';
  my $ghacks = '';
  foreach my $hack (sort { $hacks{$a} == $hacks{$b}
                           ? $a cmp $b 
                           : $hacks{$a} <=> $hacks{$b}}
                    (keys(%hacks))) {
    my $cmd = "$hack -root";
    my $ts = (length($cmd) / 8) * 8;
    while ($ts < 40) { $cmd .= "\t"; $ts += 8; }
    next if ($hack eq 'ant' || $hack eq 'rdbomb');

    my $glp;
    my $glep = ($hack eq 'extrusion');
    if (-f "$hack.c" || -f "$hack") { $glp = 0; }
    elsif (-f "glx/$hack.c") { $glp = 1; }
    else { error ("is $hack X or GL?"); }

    my $dis = ($disable{$hack} ? '-' : '');
    my $vis = ($glp
               ? (($dis ? '' : $glep ? '@GLE_KLUDGE@' : '@GL_KLUDGE@') .
                  ' GL: ')
               : '');
    $cmd = "$dis$vis\t\t\t\t$cmd    \\n\\\n";

    if ($glp) {
      ($segregate_p ? $ghacks : $xhacks) .= $cmd;
      $counts[4+defined($disable{$hack})]++;
    } else {
      $xhacks .= $cmd;
      $counts[2+defined($disable{$hack})]++;
    }
  }

  # Splice in new programs list.
  #
  $mid2 = ($xhacks .
           ($segregate_p ? "\t\t\t\t\t\t\t\t\t      \\\n" : "") .
           $ghacks);
  $mid2 =~ s@\\$@@s;
  ($top, $mid, $bot) = 
    ($body =~ m/^(.*?\n\*programs:\s+\\\n)(.*?\n)(\n.*)$/s);
  error ("unparsable") unless $mid;
  $body = $top . $mid2 . $bot;

  print STDERR "$progname: Total: $counts[0]; " .
    "X11: $counts[2]+$counts[3]; GL: $counts[4]+$counts[5]; " .
      "Names: $counts[1]\n"
        if ($verbose);

  # Write file if changed.
  #
  if ($body ne $obody) {
    local *OUT;
    open (OUT, ">$file") || error ("$file: $!");
    print OUT $body;
    close OUT;
    print STDERR "$progname: wrote $file\n";
  } elsif ($verbose) {
    print STDERR "$progname: $file unchanged\n";
  }
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose] ad-file\n";
  exit 1;
}

sub main() {
  my $file;
  while ($#ARGV >= 0) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/) { $verbose++; }
    elsif (m/^-v+$/) { $verbose += length($_)-1; }
    elsif (m/^-./) { usage; }
    elsif (!$file) { $file = $_; }
    else { usage; }
  }

  usage unless ($file);
  munge_ad ($file);
}

main();
exit 0;
