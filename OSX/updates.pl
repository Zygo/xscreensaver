#!/usr/bin/perl -w
# Copyright © 2013-2019 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# Generates updates.xml from README, archive/, and www/.
#
# Created: 27-Nov-2013.

require 5;
use diagnostics;
use strict;

use open ":encoding(utf8)";
use POSIX;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.6 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;
my $debug_p = 0;

my $base_url = "https://www.jwz.org/";
my $dsa_priv_key_file = "$ENV{HOME}/.ssh/sparkle_dsa_priv.pem";
my $dsa_sign_update = "sparkle-bin/old_dsa_scripts/sign_update";
my $edddsa_sign_update = "sparkle-bin/sign_update";


sub generate_xml($$$$) {
  my ($app_name, $changelog, $archive_dir, $www_dir) = @_;

  my $outfile = "updates.xml";

  my $obody = '';
  my %sig1s;
  my %sig2s;
  my %dates;
  if (open (my $in, '<', $outfile)) {
    print STDERR "$progname: reading $outfile\n" if $verbose;
    local $/ = undef;  # read entire file
    $obody = <$in>;
    close $in;
    my @i = split (/<item/i, $obody);
    shift @i;
    foreach my $item (@i) {
      my ($v)    = ($item =~ m/version="(.*?)"/si);
      my ($sig1) = ($item =~ m/dsaSignature="(.*?)"/si);
      my ($sig2) = ($item =~ m/edSignature="(.*?)"/si);
      my ($date) = ($item =~ m/<pubDate>(.*?)</si);
      next unless $v;
      $sig1 = '' if ($sig1 eq 'ERROR');
      $sig2 = '' if ($sig2 eq 'ERROR');
      $sig1s{$v}  = $sig1 if $sig1;
      $sig2s{$v}  = $sig2 if $sig2;
      $dates{$v} = $date if $date;
      print STDERR "$progname: existing: $v: " . ($date || '?') . "\n"
        if ($verbose > 1);
    }
  }

  open (my $in, '<', $changelog) || error ("$changelog: $!");
  print STDERR "$progname: reading $changelog\n" if $verbose;
  local $/ = undef;  # read entire file
  my $body = <$in>;
  close $in;

  my $rss = "";

  $body =~ s/^(\d+\.\d+[ \t])/\001$1/gm;
  my @log = split (/\001/, $body);
  shift @log;
  my $count = 0;
  foreach my $log (@log) {
    my ($v1, $entry) = ($log =~ m/^(\d+\.\d+)\s+(.*)$/s);

    $entry =~ s/^\s*\d\d?[- ][A-Z][a-z][a-z][- ]\d{4}:?\s+//s;  # lose date

    # unwrap continuation lines
    $entry =~ s/^[ \t]*[-*][ \t]+/\001/gm;
    $entry =~ s/\s+/ /gs;
    $entry =~ s/\001/\n* /gs;
    $entry =~ s/^\s+|\s+$//gm;

    # Since this updater is only for macOS, omit any changelog entry
    # beginning with "X11:", "Android:" etc.
    $entry =~ s/^[-*] (X11|Android|Linux|iOS): [^\n]+(\n|$)//gm;
    $entry =~ s/^([-*] )macOS: /$1/gm;

    $entry =~ s/^[-*] /<BR>&bull; /gm;
    $entry =~ s/^<BR>//si;
    $entry =~ s/\s+/ /gs;

    my $v2 = $v1; $v2 =~ s/\.//gs;
    my $zip = undef;
  DONE:
    # It only makes sense to include entries in this file for releases for
    # which a DMG still exists. Expired releases and non-macOS releases
    # aren't helpful.
    #foreach my $ext ('zip', 'dmg', 'tar.gz', 'tar.Z') {
    foreach my $ext ('dmg') {
      foreach my $v ($v1, $v2) {
        foreach my $name ($app_name, "x" . lc($app_name)) {
          my $f = "$name-$v.$ext";
          if (-f "$archive_dir/$f") {
            $zip = $f;
            last DONE;
          }
        }
      }
    }

    my $publishedp = ($zip && -f "$www_dir/$zip");
    $publishedp = 1 if ($count == 0);

    my $url = ("${base_url}$app_name/" . ($publishedp && $zip ? $zip : ""));

    $url =~ s@DaliClock/@xdaliclock/@gs if $url; # Kludge

    my @st = stat("$archive_dir/$zip") if $zip;
    my $size = $st[7];
    my $date = $st[9];
    $date = ($date ?
             strftime ("%a, %d %b %Y %T %z", localtime($date))
             : "");

    my $odate = $dates{$v1};
    my $sig1  = $sig1s{$v1};
    my $sig2  = $sig2s{$v1};
    # Re-generate the sig if the file date changed.
    $sig1 = undef if ($odate && $odate ne $date);
    $sig2 = undef if ($odate && $odate ne $date);

    print STDERR "$progname: $v1: $date " .
                  ($sig1 ? "Y" : "N") . ($sig2 ? "Y" : "N") . "\n"
      if ($verbose > 1);

    if (!$sig1 && $zip) {	# Old-style sigs
      local %ENV = %ENV;
      $ENV{PATH} = "/usr/bin:$ENV{PATH}";
      my $cmd = ("$dsa_sign_update" .
                 " \"$archive_dir/$zip\"" .
                 " \"$dsa_priv_key_file\"");
      print STDERR "$progname: exec: $cmd\n" if ($verbose > 1);
      $sig1 = `$cmd`;
      $sig1 =~ s/\s+//gs;
    }

    if (!$sig2 && $zip) {	# New-style sigs
      local %ENV = %ENV;
      $ENV{PATH} = "/usr/bin:$ENV{PATH}";
      my $cmd = "$edddsa_sign_update \"$archive_dir/$zip\"";
      print STDERR "$progname: exec: $cmd\n" if ($verbose > 1);
      my $xml = `$cmd`;
      ($sig2) = ($xml =~ m/sparkle:edSignature=\"([^\"<>\s]+)\"/si);
      error ("unparsable: $edddsa_sign_update: $xml") unless $sig2;
    }

    $sig1 = 'ERROR' unless defined($sig1);
    $sig2 = 'ERROR' unless defined($sig2);
    $size = -1 unless defined($size);
    my $enc = ($publishedp
               ? ("<enclosure url=\"$url\"\n" .
                  " sparkle:version=\"$v1\"\n" .
                  " sparkle:dsaSignature=\"$sig1\"\n" .
                  " sparkle:edSignature=\"$sig2\"\n" .
                  " length=\"$size\"\n" .
                  " type=\"application/octet-stream\" />\n")
               : "<sparkle:version>$v1</sparkle:version>\n");

    $enc =~ s/^/ /gm if $enc;
    my $item = ("<item>\n" .
                " <title>Version $v1</title>\n" .
                " <link>$url</link>\n" .
                " <description><![CDATA[$entry]]></description>\n" .
                " <pubDate>$date</pubDate>\n" .
                $enc .
                "</item>\n");
    $item =~ s/^/  /gm;

    # I guess Sparkle doesn't like info-only items.
    $item = '' unless $publishedp;

    $rss .= $item;
    $count++;
  }

  $rss = ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" .
          "<rss version=\"2.0\"\n" .
          "     xmlns:sparkle=\"http://www.andymatuschak.org/" .
               "xml-namespaces/sparkle\"\n" .
          "     xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n" .
          " <channel>\n" .
          "  <title>$app_name updater</title>\n" .
          "  <link>${base_url}$app_name/updates.xml</link>\n" .
          "  <description>Updates to $app_name.</description>\n" .
          "  <language>en</language>\n" .
          $rss .
          " </channel>\n" .
          "</rss>\n");

  if ($rss eq $obody) {
    print STDERR "$progname: $outfile: unchanged\n";
  } else {
    my $tmp = "$outfile.tmp";
    open (my $out, '>', $tmp) || error ("$tmp: $!");
    print $out $rss;
    close $out;
    if ($debug_p) {
      system ("diff", "-wNU2", "$outfile", "$tmp");
      unlink $tmp;
    } else {
      if (!rename ("$tmp", "$outfile")) {
        unlink "$tmp";
        error ("mv $tmp $outfile: $!");
      } else {
        print STDERR "$progname: wrote $outfile\n";
      }
    }
  }
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose] app-name changelog archive www\n";
  exit 1;
}

sub main() {
  binmode (STDOUT, ':utf8');
  binmode (STDERR, ':utf8');
  my ($app_name, $changelog, $archive_dir, $www_dir);
  while ($#ARGV >= 0) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/)  { $verbose++; }
    elsif (m/^-v+$/)      { $verbose += length($_)-1; }
    elsif (m/^--?debug$/) { $debug_p++; }
    elsif (m/^-./)        { usage; }
    elsif (!$app_name)    { $app_name = $_; }
    elsif (!$changelog)   { $changelog = $_; }
    elsif (!$archive_dir) { $archive_dir = $_; }
    elsif (!$www_dir)     { $www_dir = $_; }
    else { usage; }
  }

  usage unless $www_dir;
  generate_xml ($app_name, $changelog, $archive_dir, $www_dir);

}

main();
exit 0;
