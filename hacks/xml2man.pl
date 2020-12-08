#!/usr/bin/perl -w
# Copyright © 2002-2014 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# Created: 30-May-2002.
#
# This creates man pages from the XML program descriptions in 
# xscreensaver/hacks/config/.
#
# They aren't necessarily the most accurate or well-written man pages,
# but at least they exist.

require 5;
use diagnostics;
use strict;

use Text::Wrap;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.8 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;

my $default_args = ("[\\-display \\fIhost:display.screen\\fP]\n" .
                    "[\\-visual \\fIvisual\\fP]\n" .
                    "[\\-window]\n" .
                    "[\\-root]\n");
my $default_options = (".TP 8\n" .
                       ".B \\-visual \\fIvisual\\fP\n" .
                       "Specify which visual to use.  Legal values " .
                       "are the name of a visual class,\n" .
                       "or the id number (decimal or hex) of a " .
                       "specific visual.\n" .
                       ".TP 8\n" .
                       ".B \\-window\n" .
                       "Draw on a newly-created window.  " .
                       "This is the default.\n" .
                       ".TP 8\n" .
                       ".B \\-root\n" .
                       "Draw on the root window.\n");

my $man_suffix = (".SH ENVIRONMENT\n" .
                  ".PP\n" .
                  ".TP 8\n" .
                  ".B DISPLAY\n" .
                  "to get the default host and display number.\n" .
                  ".TP 8\n" .
                  ".B XENVIRONMENT\n" .
                  "to get the name of a resource file that overrides " .
                  "the global resources\n" .
                  "stored in the RESOURCE_MANAGER property.\n" .
                  ".SH SEE ALSO\n" .
                  ".BR X (1),\n" .
                  ".BR xscreensaver (1)\n" .
                  ".SH COPYRIGHT\n" .
                  "Copyright \\(co %YEAR% by %AUTHOR%.  " .
                  "Permission to use, copy, modify, \n" .
                  "distribute, and sell this software and its " .
                  "documentation for any purpose is \n" .
                  "hereby granted without fee, provided that " .
                  "the above copyright notice appear \n" .
                  "in all copies and that both that copyright " .
                  "notice and this permission notice\n" .
                  "appear in supporting documentation.  No " .
                  "representations are made about the \n" .
                  "suitability of this software for any purpose.  " .
                  "It is provided \"as is\" without\n" .
                  "express or implied warranty.\n" .
                  ".SH AUTHOR\n" .
                  "%AUTHOR%.\n");

sub xml2man($) {
  my ($exe) = @_;
  $exe =~ s/\.xml$//s;
  my $cfgdir = (-d "config" ? "config" : "../config");
  my $xml = "$cfgdir/$exe.xml";
  my $man = "$exe.man";

  error ("$exe does not exist") if (! -f $exe);
  error ("$xml does not exist") if (! -f $xml);
  error ("$man already exists") if (-f $man);

  local *IN;
  open (IN, "<$xml") || error ("$xml: $!");
  my $xmltxt = "";
  while (<IN>) { $xmltxt .= $_; }
  close IN;

  my $args = "";
  my $body = "";
  my $desc;

  $xmltxt =~ s/\s+/ /gs;
  $xmltxt =~ s/<!--.*?-->//g;
  $xmltxt =~ s@(<[^/])@\n$1@gs;

  foreach (split ('\n', $xmltxt)) {
    next if m/^$/;
    next if m/^<\?xml\b/;
    next if m/^<screensaver\b/;
    next if m/^<command\b/;
    next if m/^<[hv]group\b/;
    next if m/^<select\b/;

    my ($x,$arg) = m@\barg(|-unset|-set)=\"([^\"]+)\"@;
    my ($label)  = m@\b_?label=\"([^\"]+)\"@;
    my ($low)    = m@\blow=\"([^\"]+)\"@;
    my ($hi)     = m@\bhigh=\"([^\"]+)\"@;
    my ($def)    = m@\bdefault=\"([^\"]+)\"@;

    $arg =~ s@\s*\%\s*@ \\fInumber\\fP@g if ($arg);
                         
    my $carg = $arg;
    my $boolp = m/^<boolean/;
    my $novalsp = 0;

    if ($arg && $arg =~ m/^-no(-.*)/) {
      $arg = "$1 | \\$arg";
    } elsif ($boolp && $arg) {
      $arg = "$arg | \\-no$arg";
    }

    if ($carg && $carg =~ m/colors/) {
      $hi = $low = undef;
    }

    if (!$carg) {
    } elsif ($carg eq '-move' || $carg eq '-no-move' ||
             $carg eq '-wander' || $carg eq '-no-wander') {
      $label = "Whether the object should wander around the screen.";
    } elsif ($boolp && ($carg eq '-spin' || $carg eq '-no-spin')) {
      $label = "Whether the object should spin.";
    } elsif ($carg eq '-spin X') {
      $carg = '-spin \fI[XYZ]\fP';
      $arg = $carg;
      $label = "Around which axes should the object spin?";
    } elsif ($carg eq '-fps' || $carg eq '-no-fps') {
      $label = "Whether to show a frames-per-second display " .
               "at the bottom of the screen.";
    } elsif ($carg eq '-wireframe' || $carg eq '-wire') {
      $label = "Render in wireframe instead of solid.";
    } elsif ($carg =~ m/^-delay/ && $hi && $hi >= 10000) {
      $label = "Per-frame delay, in microseconds.";
      $def = sprintf ("%d (%0.2f seconds)", $def, ($def/1000000.0));
      $low = $hi = undef;
    } elsif ($carg eq '-speed \fInumber\fP') {
      $label = "Animation speed.  2.0 means twice as fast, " .
               "0.5 means half as fast.";
      $novalsp = 1;
    } elsif ($boolp) {
      $label .= ".  Boolean.";
    } elsif ($label) {
      $label .= ".";
    }

    if (m/^<(number|boolean|option)/) {

      next if (!$arg && m/<option/);
      if (!$label) {
        print STDERR "$progname: ERROR: no label: $_\n";
        $label = "???";
      }

      $args .= "[\\$carg]\n";

      if (! $novalsp) {
        $label .= "  $low - $hi." if (defined($low) && defined($hi));
        $label .= "  Default: $def." if (defined ($def));
      }
      $label = wrap ("", "", $label);

      $body .= ".TP 8\n.B \\$arg\n$label";
      $body .= "\n";

    } elsif (m@^<_description>\s*(.*)\s*</_description>@) {
      $desc = $1;
    } elsif (m@^<xscreensaver-updater@) {
    } elsif (m@^<video\b@) {
    } else {
      print STDERR "$progname: ERROR: UNKNOWN: $_\n";
    }
  }

  $desc = "Something pretty." unless $desc;

  my $author = undef;
  if ($desc =~ m@^(.*?)\s*(Written by|By) ([^.]+\.?\s*)$@s) {
    $desc = $1;
    $author = $3;
    $author =~ s/\s*[.]\s*$//;
  }

  if (!$author) {
    print STDERR "$progname: $exe: WARNING: unknown author\n";
    $author = "UNKNOWN";
  }

  $desc =~ s@http://en\.wikipedia\.org/[^\s]+@@gs;

  $desc = wrap ("", "", $desc);

  $body = (".TH XScreenSaver 1 \"\" \"X Version 11\"\n" .
           ".SH NAME\n" .
           "$exe \\- screen saver.\n" .
           ".SH SYNOPSIS\n" .
           ".B $exe\n" .
           $default_args .
           $args .
           ".SH DESCRIPTION\n" .
           $desc . "\n" .
           ".SH OPTIONS\n" .
           $default_options .
           $body .
           $man_suffix);

  my $year = $1 if ($author =~ s/; (\d{4})$//s);
  $year = (localtime)[5] + 1900 unless $year;

  $body =~ s/%AUTHOR%/$author/g;
  $body =~ s/%YEAR%/$year/g;

#print $body; exit 0;

  local *OUT;
  open (OUT, ">$man") || error ("$man: $!");
  print OUT $body || error ("$man: $!");
  close OUT || error ("$man: $!");
  print STDERR "$progname: wrote $man\n";
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose] programs...\n";
  exit 1;
}

sub main() {
  my @progs = ();
  while ($_ = $ARGV[0]) {
    shift @ARGV;
    if ($_ eq "--verbose") { $verbose++; }
    elsif (m/^-v+$/) { $verbose += length($_)-1; }
    elsif (m/^-./) { usage; }
    else { push @progs, $_; }
  }

  usage() if ($#progs < 0);

  foreach (@progs) { xml2man($_); }
}

main();
exit 0;
