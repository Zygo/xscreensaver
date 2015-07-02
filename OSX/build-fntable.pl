#!/usr/bin/perl -w
# Copyright © 2012-2014 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# Generates a .c file that lists all the function tables we use, because
# CFBundleGetDataPointerForName doesn't work in "Archive" builds.
# What a crock of shit.
#
# There's no real way to integrate this into the Xcode build system, so
# run this manually each time a new saver is added to the iOS app.
#
# Created: 14-Jul-2012.

require 5;
#use diagnostics;	# Fails on some MacOS 10.5 systems
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.3 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 1;

# List of savers not included in the iOS build.
#
my %disable = (
   'extrusion'		=> 1,
   'lcdscrub'		=> 1,
   'lockward'		=> 1,
   'webcollage'		=> 1,
  );

# Parse the RETIRED_EXES variable from the Makefiles to populate %disable.
# Duplicated in ../hacks/munge-ad.pl.
#
sub parse_makefiles() {
  foreach my $mf ( "../hacks/Makefile.in", "../hacks/glx/Makefile.in" ) {
    open (my $in, '<', $mf) || error ("$mf: $!");
    print STDERR "$progname: reading $mf\n" if ($verbose > 1);
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


sub build_h($) {
  my ($outfile) = @_;

  parse_makefiles();

  my @schemes = glob('xscreensaver.xcodeproj/xcuserdata/' .
                     '*.xcuserdatad/xcschemes/*.xcscheme');
  error ("no scheme files") unless (@schemes);

  my %names = ();

  foreach my $s (@schemes) {
    open (my $in, '<', $s) || error ("$s: $!");
    local $/ = undef;  # read entire file
    my $body = <$in>;
    close $in;
    my ($name) = ($body =~ m@BuildableName *= *"([^\"<>]+?)\.saver"@s);
    next unless $name;
    $name = lc($name);
    if ($disable{$name}) {
      print STDERR "$progname: skipping $name\n" if ($verbose > 1);
      next;
    }
    print STDERR "$progname: found $name\n" if ($verbose > 1);
    $names{$name} = 1;
  }

  my @names = sort (keys %names);
  error ("too few names") if (@names < 100);

  my $suf = 'xscreensaver_function_table';

  my $body = ("/* Generated file, do not edit.\n" .
              "   Created: " . localtime() . " by $progname $version.\n" .
              " */\n" .
              "\n" .
              "#import <Foundation/Foundation.h>\n" .
              "#import <UIKit/UIKit.h>\n" .
              "\n" .
              "extern NSDictionary *make_function_table_dict(void);\n" .
              "\n");

  $body .= "extern struct $suf";
  foreach my $s (@names) {
    $body .= "\n *${s}_${suf},";
  }
  $body =~ s/,\s*$/;/s;

  sub line($$) {
    my ($s, $suf) = @_;
    return "\t[NSValue valueWithPointer:&${s}_${suf}], @\"${s}\",\n";
  }

  $body .= ("\n\n" .
            "NSDictionary *make_function_table_dict(void)\n{\n" .
            "  return\n    [NSDictionary dictionaryWithObjectsAndKeys:\n" .
            "\n" .
            "#if defined(APPLE2_ONLY)\n" .
            " " . line('apple2', $suf) .
            "#elif defined(PHOSPHOR_ONLY)\n" .
            " " . line('phosphor', $suf) .
            "#else\n");
  foreach my $s (@names) { $body .= line($s, $suf); }
  $body .= ("#endif\n" .
            "\tnil];\n" .
            "}\n\n");

  my $obody = '';
  if (open (my $in, '<', $outfile)) {
    local $/ = undef;  # read entire file
    $obody = <$in>;
    close $in;
  }

  # strip comments/date for diff.
  my ($body2, $obody2) = ($body, $obody);
  foreach ($body2, $obody2) { s@/\*.*?\*/@@gs; }

  if ($body2 eq $obody2) {
    print STDERR "$progname: $outfile: unchanged\n" if ($verbose > 1);
  } else {
    my $file_tmp = "$outfile.tmp";
    open (my $out, '>', $file_tmp) || error ("$file_tmp: $!");
    print $out $body || error ("$file_tmp: $!");
    close $out || error ("$file_tmp: $!");

    if (!rename ("$file_tmp", "$outfile")) {
      unlink "$file_tmp";
      error ("mv \"$file_tmp\" \"$outfile\": $!");
    }
    print STDERR "$progname: wrote $outfile\n" if ($verbose);
  }
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose] output.c\n";
  exit 1;
}

sub main() {

  my ($out);
  while ($_ = $ARGV[0]) {
    shift @ARGV;
    if    (m/^--?verbose$/s)  { $verbose++; }
    elsif (m/^-v+$/)          { $verbose += length($_)-1; }
    elsif (m/^--?q(uiet)?$/s) { $verbose = 0; }
    elsif (m/^-/s)            { usage(); }
    elsif (! $out)            { $out = $_; }
    else                      { usage(); }
  }
  usage() unless ($out);
  build_h ($out);
}

main();
exit 0;
