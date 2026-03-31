#!/usr/bin/perl -w
# Copyright © 2026 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# Sort the files inside the XCode project into alphabetical order.
#
# Created:  9-Mar-2026.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.01 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;
my $debug_p = 0;

sub write_if_changed($$$) {
  my ($file, $body, $obody) = @_;

  if ($body eq $obody) {
    print STDERR "$progname: $file unchanged\n";
  } elsif ($debug_p) {
    my $tmp = sprintf ("/tmp/xcd-%08x", rand() * 0xFFFFFFFF);
    open (my $out, '>:utf8', $tmp) || error ("$tmp: $!");
    print $out $body;
    close $out;
    system ("diff -U4 $tmp $file");
    unlink ($tmp);
  } else {
    open (my $out, '>:utf8', $file) || error ("$file: $!");
    print $out $body;
    close $out;
    print STDERR "$progname: wrote $file\n";
  }
}

sub xcode_sort() {

  # The lists of files.
  #
  my $file = 'xscreensaver.xcodeproj/project.pbxproj';
  my $body = '';
  open (my $in, '<:utf8', $file) || error ("$file: $!");
  local $/ = undef;  # read entire file
  while (<$in>) { $body .= $_; }
  close $in;

  my $obody = $body;
  $body =~ s%( \s (?: files | children ) \s+ = \s+ \( [ \t]* \n )
             ( [^()]*? )
             ( \n [ \t]* \) )%{
    my ($aa, $bb, $cc) = ($1, $2, $3);
    if (! ($bb =~ m/(\.framework| Frameworks | Xlockmore )/)) {
      my @files = split (/\n/, $bb);
      @files = sort { my ($A, $B) = ($a, $b);
                      foreach ($A, $B) {
                        s@^ .*? /\* \s* @@sx;
                        s@^[^\s]*/@@s;
                      }
                      lc($A) cmp lc($B) } @files;
      $bb = join ("\n", @files);
    }
    "$aa$bb$cc";
  }%gsex;

  write_if_changed ($file, $body, $obody);


  # The list of schemes.
  #
  # But this doesn't work -- it keeps getting reverted, even if the change
  # happens while Xcode is not running.  Where does it *really* store this
  # data?
  #
  # Deleting xscreensaver.xcodeproj/project.xcworkspace/xcuserdata/jwz.xcuserdatad/UserInterfaceState.xcuserstate
  # doesn't help, either.
  #
  my @files = ('xscreensaver.xcodeproj/xcuserdata/' . $ENV{USER} .
               '.xcuserdatad/xcschemes/xcschememanagement.plist',
               'xscreensaver.xcodeproj/xcshareddata/xcschemes/' .
               'xcschememanagement.plist');
  foreach $file (@files) {
    $body = '';
    open ($in, '<:utf8', $file) || error ("$file: $!");
    while (<$in>) { $body .= $_; }
    close $in;
   
    $obody = $body;
  
    $body =~ s%( ^ .*? \n \t <dict> \n ) ( .*? ) ( \n \t </dict> .* ) $ %{
      my ($aa, $bb, $cc) = ($1, $2, $3);
      my @files = ($bb =~ m@ [ \t]* <key>.*?</dict> @gsx);
      @files = sort { my ($A, $B) = ($a, $b);
                        foreach ($A, $B) {
                          s@(<key>All Savers\.)@ 0 $1@;
                          s@(<key>All Savers )@ 1 $1@;
                          s@(<key>Obsolete)@ 2 $1@;
                          s@(<key>((Random)?XScreenSaver))@ 3 $1@;
                          s@(<key>jwxyz)@ 4 $1@;
                        }
                        lc($A) cmp lc($B) } @files;
      my $i = 0;
      foreach my $f (@files) {
        $f =~ s@ (<key>orderHint</key> \s* <integer> ) .*? ( </integer> )
         @$1$i$2@sx;
        $i++;
      }
      $bb = join ("\n", @files);
      "$aa$bb$cc";
    }%gsex;
  
    write_if_changed ($file, $body, $obody);
  }
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose]\n";
  exit 1;
}

sub main() {
  while (@ARGV) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/s) { $verbose++; }
    elsif (m/^-v+$/s) { $verbose += length($_)-1; }
    elsif (m/^--?debug$/s) { $debug_p++; }
    elsif (m/^-./s) { usage; }
    else { usage; }
  }

  xcode_sort();
}

main();
exit 0;
