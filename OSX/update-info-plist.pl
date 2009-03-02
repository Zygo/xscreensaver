#!/usr/bin/perl -w
# Copyright © 2006 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# Updates the NAME.xml file of a .saver bundle to include the current year,
# version number, etc.  Also updates the Info.plist file to include the
# short documentation, authors, etc. in the Finder "Get Info" properties.
#
# This is invoked by a final "Shell Script" build action on each of the
# .saver targets in the XCode project.
#
# Created:  8-Mar-2006.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my $version = q{ $Revision: 1.6 $ }; $version =~ s/^[^0-9]+([0-9.]+).*$/$1/;

my $verbose = 1;

sub read_info_plist($) {
  my ($app_dir) = @_;
  my $file = "$app_dir/Contents/Info.plist";
  $file =~ s@/+@/@g;
  local *IN;
  my $body = '';
  error ("$file: $!") unless open (IN, "<$file");
  while (<IN>) { $body .= $_; }
  close IN;
  return ($file, $body);
}


sub read_saver_xml($) {
  my ($app_dir) = @_;
  error ("$app_dir: no name") 
    unless ($app_dir =~ m@/([^/.]+).(app|saver)/?$@x);
  my $name = lc($1);
  my $file = "$app_dir/Contents/Resources/$name.xml";
  $file =~ s@/+@/@g;
  local *IN;
  my $body = '';
  error ("$file: $!") unless open (IN, "<$file");
  while (<IN>) { $body .= $_; }
  close IN;
  return ($file, $body);
}


sub update_saver_xml($$) {
  my ($app_dir, $vers) = @_;
  my ($filename, $body) = read_saver_xml ($app_dir);
  my $obody = $body;

  $body =~ m@<screensaver[^<>]*?[ \t]_label=\"([^\"]+)\"@m ||
    error ("$filename: no name label");
  my $name = $1;

  $body =~ m@<_description>(.*?)</_description>@s ||
    error ("$filename: no description tag");
  my $desc = $1;
  $desc =~ s/^([ \t]*\n)+//s;
  $desc =~ s/\s*$//s;

  # in case it's done already...
  $desc =~ s/^.* version \d[^\n]*\n//s;
  $desc =~ s/^From the XScreenSaver.*\n//m;
  $desc =~ s@^http://www\.jwz\.org/xscreensaver.*\n@@m;
  $desc =~ s/^Copyright [^ \r\n\t]+ (\d{4})(-\d{4})? (.*)\.$/Written $3; $1./m;
  $desc =~ s/^\n+//s;

  error ("$filename: description contains bad characters")
    if ($desc =~ m/([^\t\n -~]|[<>])/);

  error ("$filename: can't extract authors")
    unless ($desc =~ m@^(.*)\nWritten by[ \t]+([^\n]+)$@s);
  $desc = $1;
  my $authors = $2;
  $desc =~ s/\s*$//s;

  my $year = undef;
  if ($authors =~ m@^(.*?)\s*[,;]\s+(\d\d\d\d)([-\s,;]+\d\d\d\d)*[.]?$@s) {
    $authors = $1;
    $year = $2;
  }

  my $cyear = 1900 + ((localtime())[5]);
  $year = "$cyear" unless $year;
  if ($year && ! ($year =~ m/$cyear/)) {
    $year = "$year-$cyear";
  }

  $authors =~ s/[.,;\s]+$//s;

  # List me as a co-author on all of them, since I'm the one who
  # did the OSX port, packaged it up, and built the executables.
  #
  my $curator = "Jamie Zawinski";
  if (! ($authors =~ m/$curator/si)) {
    if ($authors =~ m@^(.*),? and (.*)$@s) {
      $authors = "$1, $2, and $curator";
    } else {
      $authors .= " and $curator";
    }
  }

  my $cc = "\302\251";  # unicode "&copy;"

  my $desc1 = ("$name, version $vers.\n\n" .
               $desc . "\n" .
               "\n" . 
               "From the XScreenSaver collection:\n" .
               "http://www.jwz.org/xscreensaver/\n" .
               "Copyright \251 $year by $authors.\n");

  my $desc2 = ("$name $vers,\n" .
               "$cc $year $authors.\n" .
               "From the XScreenSaver collection:\n" .
               "http://www.jwz.org/xscreensaver/\n" .
               "\n" .
               $desc .
               "\n");

  # unwrap lines, but only when it's obviously ok: leave blank lines,
  # and don't unwrap if that would compress leading whitespace on a line.
  #
  $desc2 =~ s/^(From |http:)/\n$1/gm;
  1 while ($desc2 =~ s/([^\s])[ \t]*\n([^\s])/$1 $2/gs);
  $desc2 =~ s/\n\n(From |http:)/\n$1/gs;

  $body =~ s@(<_description>)(.*?)(</_description>)@$1$desc1$3@s;

  if ($obody eq $body) {
    print STDERR "$progname: $filename: unchanged\n" if ($verbose > 1);
  } else {
    my $file_tmp = "$filename.tmp";
    open(OUT, ">$file_tmp") || error ("$file_tmp: $!");
    print OUT $body || error ("$file_tmp: $!");
    close OUT || error ("$file_tmp: $!");

    if (!rename ("$file_tmp", "$filename")) {
      unlink "$file_tmp";
      error ("mv \"$file_tmp\" \"$filename\": $!");
    }
    print STDERR "$progname: wrote $filename\n" if ($verbose);
  }

  return ($desc1, $desc2);
}



sub set_plist_key($$$$) {
  my ($filename, $body, $key, $val) = @_;

  if ($body =~ m@^(.*
                  \n\t<key>$key</key>
                  \n\t<string>)([^<>]*)(</string>
                  .*)$@xs) {
#    print STDERR "$progname: $filename: $key was: $2\n" if ($verbose);
    $body = $1 . $val . $3;
  } else {
    error ("$filename: unparsable")
      unless ($body =~ m@^(.*)(\n</dict>\n</plist>\n)$@s);
    $body = ($1 .
             "\n\t<key>$key</key>" .
             "\n\t<string>$val</string>" .
             $2);
  }

  return $body;
}


sub set_icon($) {
  my ($app_dir) = @_;
  $app_dir =~ s@/+$@@s;

  # "seticon" is from osxutils, http://osxutils.sourceforge.net/

  my $icon = "$app_dir/../../../XScreenSaver.icns";
  my @cmd = ("seticon", "-d", $icon, $app_dir);
  print STDERR "$progname: exec: " . join(' ', @cmd) . "\n"
    if ($verbose > 1);
  system (@cmd);
}


sub update($) {
  my ($app_dir) = @_;

  error ("$app_dir: no name") 
    unless ($app_dir =~ m@/([^/.]+).(app|saver)/?$@x);
  my $app_name = $1;

  my ($filename, $plist) = read_info_plist ($app_dir);
  my $oplist = $plist;

  error ("$filename: no version number")
    unless ($plist =~ m@<key>CFBundleShortVersionString</key>\s*
                        <string>([^<>]+)</string>@sx);
  my $vers = $1;
  my ($ignore, $info_str) = update_saver_xml ($app_dir, $vers);

  $info_str =~ m@^([^\n]+)\n@s ||
    error ("$filename: unparsable copyright");
  my $copyright = "$1";
  $copyright =~ s/\b\d{4}-(\d{4})\b/$1/;

  $plist = set_plist_key ($filename, $plist, 
                          "NSHumanReadableCopyright", $copyright);
  $plist = set_plist_key ($filename, $plist,
                          "CFBundleLongVersionString",$copyright);
  $plist = set_plist_key ($filename, $plist,
                          "CFBundleGetInfoString",    $info_str);

  if ($oplist eq $plist) {
    print STDERR "$progname: $filename: unchanged\n" if ($verbose > 1);
  } else {
    my $file_tmp = "$filename.tmp";
    open(OUT, ">$file_tmp") || error ("$file_tmp: $!");
    print OUT $plist || error ("$file_tmp: $!");
    close OUT || error ("$file_tmp: $!");

    if (!rename ("$file_tmp", "$filename")) {
      unlink "$file_tmp";
      error ("mv \"$file_tmp\" \"$filename\": $!");
    }
    print STDERR "$progname: wrote $filename\n" if ($verbose);
  }

  set_icon ($app_dir);
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose] program.app ...\n";
  exit 1;
}

sub main() {
  my @files = ();
  while ($#ARGV >= 0) {
    $_ = shift @ARGV;
    if ($_ eq "--verbose") { $verbose++; }
    elsif (m/^-v+$/) { $verbose += length($_)-1; }
    elsif (m/^-./) { usage; }
    else { push @files, $_; }
  }
  usage() unless ($#files >= 0);
  foreach (@files) {
    update ($_);
  }
}

main();
exit 0;
