#!/opt/local/bin/perl -w
# Copyright © 2025 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# Created: 25-Feb-2025.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.2 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;

BEGIN { eval 'use Perl::Tidy;' }

sub minimize($$) {
  my ($in, $out) = @_;

  if (!defined($Perl::Tidy::VERSION)) {
    print STDERR "$progname: Perl::Tidy not installed, skipping...\n";
    return;
  }

  open (my $inf, "<:utf8", $in) || error ("$in: $!");
  local $/ = undef;  # read entire file
  my $body = <$inf>;
  close $inf;

  my $body2;
  my $err = Perl::Tidy::perltidy (
    argv        => join (' ',
                         '--mangle',
                         '--delete-all-comments',
			),
    source      => \$body,
    destination => \$body2,
    );
  error ($err) if $err;

  # Find all functions and variables, and shorten them.
  #
  my @subs = ($body2 =~ m/ \b sub \s+ ([a-zA-Z\d_]+) \s* [\{\(] /gsx);
  my @vars = ($body2 =~ m/ \b my \s* [\$#%@] ([a-zA-Z\d_]+) [\s;,=] /gsx);
  my @args = ($body2 =~ m/ \b my \s* \( ( .*? ) \) /gsx);

  foreach my $tt (@args) {
    foreach my $t (split (/\s*,\s*/, $tt)) {
      $t =~ s/^[\$#%@]//s;
      $t =~ s/=.*//s;
      push @vars, $t;
    }
  }

  my %tokens;
  my $i = 0;
  foreach my $t (@vars) {
    if (length($t) < 4) {
      print STDERR "$progname: skip var $t\n" if ($verbose > 1);
      next;
    }
    $tokens{$t} = sprintf ("V%X", $i++);
  }

  $i = 0;
  foreach my $t (@subs) {
    if (length($t) < 4) {
      print STDERR "$progname: skip sub $t\n" if ($verbose > 1);
      next;
    }
    next if (length($t) < 4);
    $tokens{$t} = sprintf ("F%X", $i++);
  }

  foreach my $k (sort keys %tokens) {
    my $v = $tokens{$k};
    if ($v =~ m/^S/) {
      $body2 =~ s/ ( [^a-zA-Z\d_] ) ( $k ) ( [^a-zA-Z\d_] ) /$1$v$3/gsx;
    } else {
      $body2 =~ s/ ( [\$#%@] \{? )  ( $k ) ( [^a-zA-Z\d_] ) /$1$v$3/gsx;
    }
    print STDERR "$progname: $k => $v\n" if ($verbose > 1);
  }

  $in =~ s@^.*/@@s;
  my $tag = "# DO NOT EDIT -- auto-generated from \"$in\"\n\n";
  $body2 =~ s/^([^\n]+\n)/$1$tag/s;

  $body2 =~ s/\n\n+/\n/gs;  # Why is it double-spaced?

  if ($out eq '-') {
    print STDOUT $body2;
  } else {
    open (my $outf, ">:utf8", $out) || error ("$out: $!");
    print $outf $body2;
    close $outf;
    print STDERR "$progname: wrote $out\n";
  }
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose] in.pl out.pl\n";
  exit 1;
}

sub main() {
  my ($in, $out);
  while (@ARGV) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/s) { $verbose++; }
    elsif (m/^-v+$/s) { $verbose += length($_)-1; }
#   elsif (m/^--?debug$/s) { $debug_p++; }
    elsif (m/^-./s) { usage; }
    elsif (!$in)  { $in  = $_; }
    elsif (!$out) { $out = $_; }
    else { usage; }
  }

  usage unless ($out);
  minimize ($in, $out);
}

main();
exit 0;
