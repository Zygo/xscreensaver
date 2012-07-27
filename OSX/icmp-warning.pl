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
# Created: 20-Jun-2012.

require 5;
#use diagnostics;	# Fails on some MacOS 10.5 - 10.7 systems
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my $version = q{ $Revision: 1.2 $ }; $version =~ s/^[^\d]+([\d.]+).*/$1/;

my $verbose = 0;

sub sanity_check() {

  my $fail = '';
  my $d1 = $ENV{SDK_DIR} || '';
  my $d2 = '/usr/include/netinet/';
  foreach my $f ('ip.h', 'in_systm.h', 'ip_icmp.h', 'ip_var.h', 'udp.h') {
    $fail .= "\tsudo ln -s $d2$f $d1$d2\n"
      unless (-f "$d1$d2$f");
  }

  exit (0) unless $fail;

  print STDERR "ERROR:\t" . join(' ',  # "\n\t",
     'The "Sonar" module won\'t build properly unless you repair your',
     'SDK first.  With some versions of Xcode, the ICMP header files',
     'are present in the iPhone Simulator SDK but are missing from',
     'the "real device" SDK.  You can fix it by doing this:') .
       "\n\n$fail\n";
  exit (1);
}

if ($#ARGV >= 0) {
  print STDERR "usage: $progname\n";
  exit 1;
}

sanity_check();
