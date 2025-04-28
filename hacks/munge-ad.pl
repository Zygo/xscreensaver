#!/usr/bin/perl -w
# Copyright © 2008-2022 Jamie Zawinski <jwz@jwz.org>
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
my ($version) = ('$Revision: 1.16 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;

# 1 means disabled: marked with "-" by default in the .ad file.
# 2 means retired: not mentioned in .ad at all (parsed from Makefile).
#
my %disable = ( 
   'antinspect'		=> 1,
   'antmaze'		=> 1,
   'antspotlight'	=> 1,
   'braid'		=> 1,
   'co____9'		=> 2,
   'crystal'		=> 1,
   'demon'		=> 1,
   'dnalogo'		=> 1,
   'fadeplot'		=> 1,
   'glblur'		=> 1,
   'glslideshow'	=> 1,
   'jigglypuff'		=> 1,
   'kaleidescope'	=> 1,
   'lcdscrub'		=> 1,
   'loop'		=> 1,
   'mismunch'		=> 2,
   'nerverot'		=> 1,
   'noseguy'		=> 1,
   'polyominoes'	=> 1,
   'providence'		=> 1,
   'pyro'		=> 1,
   'rocks'		=> 1,
   'sballs'		=> 1,
   'sierpinski'		=> 1,
   'testx11'		=> 2,
   'thornbird'		=> 1,
   'vidwhacker'		=> 1,
   'webcollage'		=> 1,
  );


# Parse the RETIRED_EXES variable from the Makefiles to populate %disable.
#
sub parse_makefiles() {
  foreach my $mf ( "Makefile.in", "glx/Makefile.in" ) {
    open (my $in, '<:utf8', $mf) || error ("$mf: $!");
    local $/ = undef;  # read entire file
    my $body = <$in>;
    close $in;

    $body =~ s/\\\n//gs;
    my ($var)  = ($body =~ m/^RETIRED_EXES\s*=\s*(.*)$/mi);
    my ($var2) = ($body =~ m/^RETIRED_GL_EXES\s*=\s*(.*)$/mi);
    error ("no RETIRED_EXES in $mf") unless $var;
    $var .= " $var2" if $var2;
    foreach my $hack (split (/\s+/, $var)) {
      $disable{$hack} = 2;
    }
  }
}


sub munge_ad($) {
  my ($file) = @_;

  parse_makefiles();

  open (my $in, '<:utf8', $file) || error ("$file: $!");
  local $/ = undef;  # read entire file
  my $body = <$in>;
  close $in;
  my $obody = $body;

  my ($top, $mid, $bot) = ($body =~ m/^(.*?\n)(\*hacks\..*?\n)(\n.*)$/s);

  my $mid2 = '';

  my %hacks;

  # Update the "*hacks.foo.name" section of the file based on the contents
  # of config/*.xml.
  #
  my $dir = $file;
  $dir =~ s@/[^/]*$@@s;
  my @counts = (0,0,0,0,0,0,0,0,0,0);
  foreach my $xml (sort (glob ("$dir/../hacks/config/*.xml"))) {
    open (my $in, '<:utf8', $xml) || error ("$xml: $!");
    local $/ = undef;  # read entire file
    my $b = <$in>;
    close $in;
    my ($name)  = ($b =~ m@<screensaver[^<>]* \b name   = \" ([^<>\"]+) \"@sx);
    my ($label) = ($b =~ m@<screensaver[^<>]* \b _label = \" ([^<>\"]+) \"@sx);
    error ("$xml: no name") unless $name;
    error ("$xml: no label") unless $label;

    my $label2 = lc($name);
    $label2 =~ s/^((x|gl)?[a-z])/\U$1/s;  # what prefs.c (make_hack_name) does

    if ($label ne $label2) {
      my $s = sprintf("*hacks.%s.name:", $name);
      $mid2 .= sprintf ("%-28s%s\n", $s, $label);
      $counts[9]++;
    }

    # Grab the year.
    my ($year) =
      ($b =~ m/<_description>.*Written by.*?;\s+(19[6-9]\d|20\d\d)\b/si);
    error ("no year in $name.xml") unless $year;
    $hacks{$name} = $year;
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
    my $cmd = "$hack --root";
    my $ts = (length($cmd) / 8) * 8;
    while ($ts < 40) { $cmd .= "\t"; $ts += 8; }

    my $dis = $disable{$hack} || 0;

    my $glp;
    my $glep = ($hack eq 'extrusion');
    if (-f "$hack.c" || -f "$hack") { $glp = 0; }
    elsif (-f "glx/$hack.c") { $glp = 1; }
    elsif ($hack eq 'companioncube') { $glp = 1; }  # kludge
    elsif ($dis != 2) { error ("is $hack X or GL?"); }

    $counts[($disable{$hack} || 0)]++;
    if ($glp) {
      $counts[6+($disable{$hack} || 0)]++;
    } else {
      $counts[3+($disable{$hack} || 0)]++;
    }

    next if ($dis == 2);

    $dis = ($dis ? '-' : '');
    my $vis = ($glp
               ? (($dis ? '' : $glep ? '@GLE_KLUDGE@' : '@GL_KLUDGE@') .
                  ' GL: ')
               : '');
    $cmd = "$dis$vis\t\t\t\t$cmd    \\n\\\n";

    if ($glp) {
      ($segregate_p ? $ghacks : $xhacks) .= $cmd;
    } else {
      $xhacks .= $cmd;
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

  print STDERR "$progname: " .
    "Total: $counts[0]+$counts[1]+$counts[2]; " .
      "X11: $counts[3]+$counts[4]+$counts[5]; " .
       "GL: $counts[6]+$counts[7]+$counts[8]; " .
    "Names: $counts[9]\n"
        if ($verbose);

  # Write file if changed.
  #
  if ($body ne $obody) {
    open (my $out, '>:utf8', $file) || error ("$file: $!");
    print $out $body;
    close $out;
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
