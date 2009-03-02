#!/usr/bin/perl

#------------------------------------------------------------------------------
#  @(#)genbuild.pl 4.0 2000/01/21 xlockmore
#
#  mode management include file generator for xlock
#
#  Originally written in korn shell and lex and yacc by
#  Charles Vidal (make.launcher)
#  Copyright (c) by Charles Vidal
#
#  See xlock.c for copying information.
#
#  Revision History:
#
#  21-Jan-2000: converted to perl by David Bagley, for ease adding to list
#  10-Oct-1999: xglock generation  by Eric Lassauge <lassauge AT users.sourceforge.net>
#  ??-???-??: Written by Charles Vidal <vidalc@club-internet.fr>
#
#------------------------------------------------------------------------------

my(@GUI_LIST) = (
  "MOTIF,xmlock.modes.tpl,../../xmlock/modes.h,1",
  "GTK,xglock.modes.tpl,../../xglock/modes.h,1",
  "TCL,etc.xlock.tcl,../xlock.tcl,0",
  "JAVA,etc.xlock.java,../xlockFrame.java,0");
my($GUI_NAME, $GUI_TPL, $GUI_FILE, $GUI_BUILD);
my($GUI, $PROG, $CMD, $LIB);

$datafile = "lmode.h";

$PROG=xlockgen;
if (-x "$PROG") {
  foreach $GUI (@GUI_LIST) {
    ($GUI_NAME, $GUI_TPL, $GUI_FILE, $GUI_BUILD) = split (/,/, $GUI);
#    print("$GUI_NAME, $GUI_TPL, $GUI_FILE, $GUI_BUILD\n");
#    print "make $GUI_NAME in 2 passes:\n";
    print "generating $GUI_NAME\n";
    if (-w "$GUI_FILE") {
      if (-r "$GUI_TPL") {
        $CMD="cat $GUI_TPL | ./xlockgen > $GUI_FILE";
        print "$CMD\n";
        `$CMD`;
        if ($GUI_BUILD) {
          print "Can now build $GUI_NAME.\n";
        } else {
          print "$GUI_NAME program now configured.\n";
        }
      } else {
        print "Could not open $GUI_TPL for reading\n";
      }
    } else {
      print "Could not open $GUI_FILE for writing\n";
    }
  }
} else {
  print "Could not execute $PROG, maybe it needs to be compiled?\n";
  print " using lex: gcc lex.yy.c -o $PROG -ll\n";
  print " using flex: gcc lex.yy.c -o $PROG -lfl\n";
}



exit;

# this other code does the same thing without lex
foreach $GUI (@GUI_LIST) {
  ($GUI_NAME, $GUI_TPL, $GUI_FILE, $GUI_BUILD) = split (/,/, $GUI);
  print "generating $GUI_NAME\n";
  if (-w "$GUI_FILE") {
    if (-r "$GUI_TPL") {
      if ($GUI_NAME eq "MOTIF") {
        &buildmotif ($GUI_NAME, $GUI_TPL, $datafile, $GUI_FILE,);
      } elsif ($GUI_NAME eq "GTK") {
        &buildgtk ($GUI_NAME, $GUI_TPL, $datafile, $GUI_FILE,);
      } elsif ($GUI_NAME eq "TCL") {
        &buildtcl ($GUI_NAME, $GUI_TPL, $datafile, $GUI_FILE);
      } elsif ($GUI_NAME eq "JAVA") {
        &buildjava ($GUI_NAME, $GUI_TPL, $datafile, $GUI_FILE);
      }
      print "can now build $GUI_NAME\n";
    } else {
      print "Could not open $GUI_TPL for reading\n";
    }
  } else {
    print "Could not open $GUI_FILE for writing\n";
  }
}

# to help adminitration and utils for launcher(s)
# this file replace token by all modes token :
# LISTMOTIF, LISTTCL, LISTGTK
# utils :

@Gui_Types=(
  "\$\%LISTMOTIF", "\$\%LISTGTK",
  "\$\%LISTTCL", "\$\%LISTJAVA");


sub buildmotif
{
  $name = $_[0];
  $templatefile = $_[1];
  $datafile = $_[2];
  $outfile = $_[3];

  open(TEMPLATE, "<$templatefile") || die("Could not open $templatefile for reading");
  open(OUTFILE, ">$outfile") || die("Could not open $outfile for writing");
  while (<TEMPLATE>) {
    if (/^.*LISTMOTIF(.*)/) {
      $restOfLine = "$1\n";
      $_ = "$1\n";
      open(DATA, "<$datafile") || die("Could not open $datafile for reading");
      $instruct = 0;
      while(<DATA>) {
        chop();
        if ($instruct) {
          if (/^.*};/) {
            $instruct = 0;
          } else {
            if ($mode == 0) {
              if (/^\s*{\"(.*)\",/) {
                $name = $1;
                $mode++;
              }
            } elsif ($mode == 1) {
              $mode++;
            } else {
              if (/^\s*\"(.*)\",.*,\s*"(.*)"}/) {
                print OUTFILE "\(char \*\) $2\n{\"$name\", \(char \*\) \"$1\"},\n#endif\n";
              } elsif (/^\s*\"(.*)\",.*,\s*(.*)}/) { #NULL
                print OUTFILE "{\(char \*\) \"$name\", \(char \*\) \"$1\"},\n";
              } else {
                print OUTFILE "#$_#\n";
              }
              $mode = 0;
            }
          }
        } else {
          if (/^.*LockProcs\[\]\s*=/) {
            $instruct = 1;
            $mode = 0;
          }
        }
      }
      close(DATA);
      $_ = $restOfLine;
    }
    print OUTFILE "$_";
  }
  close(TEMPLATE);
  close(OUTFILE);
}

sub buildgtk
{
  $name = $_[0];
  $templatefile = $_[1];
  $datafile = $_[2];
  $outfile = $_[3];

  open(TEMPLATE, "<$templatefile") || die("Could not open $templatefile for reading");
  open(OUTFILE, ">$outfile") || die("Could not open $outfile for writing");
  while (<TEMPLATE>) {
    if (/^.*LISTGTK(.*)/) {
      $restOfLine = "$1\n";
      $_ = "$1\n";
      open(DATA, "<$datafile") || die("Could not open $datafile for reading");
      $instruct = 0;
      while(<DATA>) {
        chop();
        if ($instruct) {
          if (/^.*};/) {
            $instruct = 0;
          } else {
            if ($mode == 0) {
              if (/^\s*{\"(.*)\",/) {
                $name = $1;
                $mode++;
              }
            } elsif ($mode == 1) {
              if (/^\s*(.*),\s*(.*),\s(.*),\s*(.*),\s(.*),\s(.*),/) {
                $delay = $1;
                $count = $2;
                $cycles = $3;
                $saturation = $5;
              }
              $mode++;
            } else {
              if (/^\s*\"(.*)\",.*,\s*"(.*)"}/) {
                print OUTFILE "$2\n  {\"$name\",\n";
                print OUTFILE "   $delay, $count, $cycles, $saturation,\n";
                print OUTFILE "   \"$1\", (void *) NULL},\n#endif\n";
              } elsif (/^\s*\"(.*)\",.*,\s*(.*)}/) { #NULL
                print OUTFILE "  {\"$name\",\n";
                print OUTFILE "   $delay, $count, $cycles, $saturation,\n";
                print OUTFILE "   \"$1\", (void *) NULL},\n";
              } else {
                print OUTFILE "#$_#\n";
              }
              $mode = 0;
            }
          }
        } else {
          if (/^.*LockProcs\[\]\s*=/) {
            $instruct = 1;
            $mode = 0;
          }
        }
      }
      close(DATA);
      $_ = $restOfLine;
    }
    print OUTFILE "$_";
  }
  close(TEMPLATE);
  close(OUTFILE);
}

sub buildtcl
{
  $name = $_[0];
  $templatefile = $_[1];
  $datafile = $_[2];
  $outfile = $_[3];

  open(TEMPLATE, "<$templatefile") || die("Could not open $templatefile for reading");
  open(OUTFILE, ">$outfile") || die("Could not open $outfile for writing");
  while (<TEMPLATE>) {
    if (/^.*LISTTCL(.*)/) {
      $restOfLine = "$1\n";
      $_ = "$1\n";
      open(DATA, "<$datafile") || die("Could not open $datafile for reading");
      $instruct = 0;
      while(<DATA>) {
        if ($instruct) {
          if (/^.*};/) {
            $instruct = 0;
          } else {
            if (/^\s*{\"(.*)\",/) {
              print OUTFILE "$1\\\n";
            }
          }
        } else {
          if (/^.*LockProcs\[\]\s*=/) {
            $instruct = 1;
          }
        }
      }
      close(DATA);
      $_ = $restOfLine;
    }
    print OUTFILE "$_";
  }
  close(TEMPLATE);
  close(OUTFILE);
}

sub buildjava
{
  $name = $_[0];
  $templatefile = $_[1];
  $datafile = $_[2];
  $outfile = $_[3];

  open(TEMPLATE, "<$templatefile") || die("Could not open $templatefile for reading");
  open(OUTFILE, ">$outfile") || die("Could not open $outfile for writing");
  while (<TEMPLATE>) {
    if (/^.*LISTJAVA(.*)/) {
      $restOfLine = "$1\n";
      $_ = "$1\n";
      open(DATA, "<$datafile") || die("Could not open $datafile for reading");
      $instruct = 0;
      while(<DATA>) {
        if ($instruct) {
          if (/^.*};/) {
            $instruct = 0;
          } else {
            if (/^\s*{\"(.*)\",/) {
              print OUTFILE "lst.addItem(\"$1\");\n";
            }
          }
        } else {
          if (/^.*LockProcs\[\]\s*=/) {
            $instruct = 1;
          }
        }
      }
      close(DATA);
      $_ = $restOfLine;
    }
    print OUTFILE "$_";
  }
  close(TEMPLATE);
  close(OUTFILE);
}
