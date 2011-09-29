#!/usr/bin/perl -w
# Copyright © 2008-2011 Jamie Zawinski <jwz@jwz.org>
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
my $version = q{ $Revision: 1.7 $ }; $version =~ s/^[^\d]+([\d.]+).*/$1/;

my $verbose = 0;

# 1 means disabled: marked with "-" by default in the .ad file.
# 2 means retired: not mentioned in .ad at all.
#
my %disable = ( 
   'abstractile'	=> 1,
   'antinspect'		=> 1,
   'antmaze'		=> 1,
   'antspotlight'	=> 1,
   'braid'		=> 1,
   'crystal'		=> 1,
   'demon'		=> 1,
   'dnalogo'		=> 1,
   'fadeplot'		=> 1,
   'glblur'		=> 1,
   'glplanet'		=> 1,
   'glslideshow'	=> 1,
   'jigglypuff'		=> 1,
   'juggle'		=> 2,
   'kaleidescope'	=> 1,
   'lcdscrub'		=> 1,
   'loop'		=> 1,
   'mismunch'		=> 2,
   'nerverot'		=> 1,
   'noseguy'		=> 1,
   'polyominoes'	=> 1,
   'providence'		=> 1,
   'pyro'		=> 1,
   'rdbomb'		=> 2,  # alternate name
   'rocks'		=> 1,
   'sballs'		=> 1,
   'sierpinski'		=> 1,
   'thornbird'		=> 1,
   'vidwhacker'		=> 1,
   'webcollage'		=> 1,
   'xsublim'		=> 2,
  );


# Parse the RETIRED_EXES variable from the Makefiles to populate %disable.
#
sub parse_makefiles() {
  foreach my $mf ( "Makefile.in", "glx/Makefile.in" ) {
    my $body = '';
    local *IN;
    open (IN, "<$mf") || error ("$mf: $!");
    while (<IN>) { $body .= $_; }
    close IN;

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
  my @counts = (0,0,0,0,0,0,0,0,0,0);
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
      $counts[9]++;
    }

    # Grab the year.
    my ($year) =
      ($b =~ m/<_description>.*Written by.*?;\s+(19[6-9]\d|20\d\d)\b/si);
    error ("no year in $xml.xml") unless $year;
    $hacks{$xml} = $year;
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
