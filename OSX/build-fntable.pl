#!/usr/bin/perl -w
# Copyright © 2012 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# Generates a .h file that lists all the function tables we use, because
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
my $version = q{ $Revision: 1.1 $ }; $version =~ s/^[^0-9]+([0-9.]+).*$/$1/;

my $verbose = 1;

sub build_h($$) {
  my ($app_dir, $outfile) = @_;

  opendir (my $dh, $app_dir) || error ("$app_dir: $!");
  print STDERR "$progname: scanning $app_dir...\n" if ($verbose > 1);

  my @names = ();
  foreach (sort (readdir ($dh))) {
    next unless (m/^(.*)\.xml$/);
    push @names, $1;
  }
  closedir $dh;

  my $suf = 'xscreensaver_function_table';

  my $body = "extern struct $suf";
  foreach my $s (@names) {
    $body .= "\n *${s}_${suf},";
  }
  $body =~ s/,\s*$/;/s;

  $body .= ("\n\n" .
            "static NSDictionary *make_function_tables_dict(void)\n{\n" .
            "  return\n    [NSDictionary dictionaryWithObjectsAndKeys:\n");
  foreach my $s (@names) {
    $body .= "\t[NSValue valueWithPointer:&${s}_${suf}], @\"${s}\",\n";
  }
  $body .= ("\tnil];\n" .
            "}\n");

  my $obody = '';
  if (open (my $in, '<', $outfile)) {
    local $/ = undef;  # read entire file
    $obody = <$in>;
    close $in;
  }

  if ($obody eq $body) {
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
  print STDERR "usage: $progname [--verbose] program.app output.h\n";
  exit 1;
}

sub main() {

  my ($app, $out);
  while ($_ = $ARGV[0]) {
    shift @ARGV;
    if    (m/^--?verbose$/s)  { $verbose++; }
    elsif (m/^-v+$/)          { $verbose += length($_)-1; }
    elsif (m/^--?q(uiet)?$/s) { $verbose = 0; }
    elsif (m/^-/s)            { usage(); }
    elsif (! $app)            { $app = $_; }
    elsif (! $out)            { $out = $_; }
    else                      { usage(); }
  }
  usage() unless ($out && $app);
  build_h ($app, $out);
}

main();
exit 0;
