#!/usr/bin/perl -w
# Copyright © 2012-2025 Jamie Zawinski <jwz@jwz.org>
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
my ($version) = ('$Revision: 1.15 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 1;

# List of savers not included in the iOS build.
#
my %disable = (
   'extrusion'		=> 1,
   'flurry'		=> 1,
   'glitchpeg'		=> 1,
   'lcdscrub'		=> 1,
   'lockward'		=> 1,
   'mapscroller'	=> 1,
   'webcollage'		=> 1,
   'testx11'		=> 1,
   'covid19'		=> 1,  # Fuck you, Apple.
  #'co____9'		=> 1,  # Double-fuck you.
   'xscreensaver-getimage' => 1,
  );

# Parse specified variables from a Makefile.
# ../hacks/munge-ad.pl also parses Makefiles.
#
sub parse_makefile_vars($@)
{
  my ($mf, @vars) = @_;

  open (my $in, '<:utf8', $mf) || error ("$mf: $!");
  print STDERR "$progname: reading $mf\n" if ($verbose > 1);
  local $/ = undef;  # read entire file
  my $body = <$in>;
  close $in;

  my %vars = map { $_ => 1 } @vars;
  my %result;
  while ($body =~ /^([^:#=\s]+)\s*=\s*((?:\\\n|.)*)/mg)
  {
    my ($key, $value) = ($1, $2);
    if (!%vars || $vars{$key}) {
      $value =~ s/\\\n/ /g;
      $result{$key} = $value;
    }
  }
  return \%result;
}


sub build_h($) {
  my ($outfile) = @_;

  my %names = ();

  my @exes =
    (values %{ parse_makefile_vars ('../hacks/Makefile.in', 'EXES') },
     values %{ parse_makefile_vars ('../hacks/glx/Makefile.in', 'GL_EXES',
                                    'SUID_EXES') });
  my @slexes = values %{ parse_makefile_vars ('../hacks/glx/Makefile.in',
                                              'GLSL_EXES') };
  my %slexes;
  foreach my $var (@slexes) {
    foreach my $name (split (/\s+/, $var)) {
      $slexes{$name} = 1;
    }
  }

  foreach my $var (@exes, @slexes) {
    $var =~ s/covid19/co____9/gs;
    foreach my $name (split (/\s+/, $var)) {
      if ($name =~ /@/ || $disable{$name}) {
        print STDERR "$progname: skipping $name\n" if ($verbose > 1);
        next;
      }
      print STDERR "$progname: found $name\n" if ($verbose > 1);
      $names{$name} = 1;
    }
  }

  $names{'dnalogo'} = 1;  # Not listed in GL_EXES

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
  foreach my $s (@names, 'testx11') {
    next if ($slexes{$s});
    my $s2 = $s;
    $s2 =~ s/-/_/g;
    $body .= "\n ${s2}_${suf},";
  }
  $body =~ s/,\s*$/;/s;

  sub line($$) {
    my ($s, $suf) = @_;

    next if ($s eq 'xshadertoy');
    my $xml_file = "../hacks/config/$s.xml";
    open (my $in, '<:utf8', $xml_file) || error ("$xml_file: $!");
    local $/ = undef;  # read entire file
    my $body = <$in>;
    close $in;
    my ($title) = ($body =~ m@<screensaver[^<>]*?[ \t]_label=\"([^\"]+)\"@m);
    error ("$xml_file: no title") unless $title;

    $s = 'xshadertoy' if ($slexes{$s});
    return "\t@\"${title}\":\t[NSValue valueWithPointer:&${s}_${suf}],\n";
  }

  $body .= ("\n\n" .
            "NSDictionary *make_function_table_dict(void) {\n" .
            "  return \@{\n" .
            "#if defined(APPLE2_ONLY)\n" .
              line('apple2', $suf) .
            "#elif defined(PHOSPHOR_ONLY)\n" .
              line('phosphor', $suf) .
            "#elif defined(TESTX11_ONLY)\n" .
              line('testx11', $suf) .
            "#else\n");
  foreach my $s (@names) { $body .= line($s, $suf); }
  $body .= ("#endif\n" .
            "\t};\n" .
            "}\n\n");

  my $obody = '';
  if (open (my $in, '<:utf8', $outfile)) {
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
    open (my $out, '>:utf8', $file_tmp) || error ("$file_tmp: $!");
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
