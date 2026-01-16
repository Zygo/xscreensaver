#!/usr/bin/perl -w
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
# Download source code from shadertoy.com.
#
# Usage: xshadertoy-download.pl --cookie 'XXX' URL
#
# It will write files like:
#
#   shadername-common.glsl
#   shadername-0.glsl
#   shadername-1.glsl
#   shadername-2.glsl
#
# You need to pull the --cookie argument out of your web browser.
# Also it only lasts an hour or so.  This sucks.
#
# Created: 19-Dec-2025.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.02 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 1;
my $debug_p = 0;

use POSIX;
use IPC::Open2;
use LWP::UserAgent;
use JSON::Any;


sub url_quote($) {
  my ($u) = @_;
  $u =~ s|([^-a-zA-Z0-9.\@_\r\n])|sprintf("%%%02X", ord($1))|ge;
  return $u;
}

sub cmdstr(@) {
  my @cmd = @_;

  # Quote the string with single quotes, which does not require backslashes
  # for any special character; quote single quote as '\''
  #
  my $s = join(' ', map { if ($_ eq '' || m![^-._,:a-z\d/@+=]!s) {
                                     s%\'%'\\''%gsi;
                                     s/\n/\\n/gsi;
                                     $_ = "\'$_\'";
                                   }
                                   $_;
                                 } @cmd);

  # Make multi-line commands more readable.
  $s =~ s/('\(')/ \\\n  $1/gs;
  $s =~ s/('\)')/$1 \\\n  /gs;
  $s =~ s/(\\)(\n *\\)+/$1/gs;
  $s =~ s/(\s-( i | vframes | t | c:v | acodec | max_muxing_queue_size |
                loop | map | pass | b:v | c:s | passlogfile |
                vf | af | filter_complex | movflags | metadata[^\s]* |
                c:a ) \s )
         / \\\n$1/gsx;
  $s =~ s/( '[^ ]+$)/ \\\n$1/s;

  return $s;
}

sub json_decode($) {
  my ($s) = @_;
  my $json = undef;
  eval {
    my $j = JSON::Any->new;
    $json = $j->jsonToObj ($s);
  };
  return $json;
}


sub load($$$$) {
  my ($id, $url, $post, $cookie) = @_;

  $cookie =~ s/^Cookie: //si;

  my $agent = ('User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)' .
               ' AppleWebKit/605.1.15 (KHTML, like Gecko)' .
               ' Version/18.6 Safari/605.1.15');
  my $body = '';
  if (0) {
    # Somehow Clownflare is blocking both LWP and wget, but curl works...
  
    my $ua = LWP::UserAgent->new;
    $ua->agent($agent);
  
    if ($verbose > 3) {
      $ua->add_handler("request_send",  sub { shift->dump; return });
      $ua->add_handler("response_done", sub { shift->dump; return });
    }
  
    my $res = ($post
               ? $ua->post ($url,
                            Content_Type => 'application/x-www-form-urlencoded',
                            Referer      => $url,
                            Cookie       => $cookie,
                            Content      => $post,
                            )
               : $ua->get ($url,
                           Referer      => $url,
                           Cookie       => $cookie,
                          ));
    my $ret = ($res && $res->code) || 'null';
    if (!$res->is_success) {
      my $err = "$id: status $ret";
      # $err .= $find if ($ret == 403);
      # error ($err);
      return '';
    }

    $body = $res->decoded_content;
  } else {
    my @cmd = ('curl',
               $url,
               '--silent',
               '--header', $agent,
               '--header', "Referer: $url",
               '--header', "Cookie: $cookie",
               ($post
                ? ('--request', 'POST',
                   '--header',
                     'Content-Type: application/x-www-form-urlencoded',
                   '--data', $post)
                : ()));

    print STDERR "$progname: exec: " . cmdstr(@cmd) . "\n" if ($verbose > 1);
    my ($in, $out);
    my $pid = open2 ($out, $in, @cmd);
    close ($in);
    while (<$out>) { $body .= $_; }
    waitpid ($pid, 0);
    my $status = $? >> 8;
    error ("$id: failed with status $status") if ($status);

    close ($out);
  }

  return $body;
}

sub download($$) {
  my ($url, $cookie) = @_;

  my ($id) = ($url =~ m@/view/([^/?&]{4,})$@s);
  error ("unparsable shadertoy URL: $url") unless $id;

  my $url2 = 'https://www.shadertoy.com/shadertoy';

  my $find = join("\n\t", "", "",
                  "Re-invoke this script with the '--cookie' parameter.",
                  "",
                  "To find your cookie:",
                  "",
                  "  Open the URL in a browser; Show Web Inspector;" .
                   " Sources tab;",
                  "  Copy the value of the Cookie header on the right.");
  my $post = join ('&',
                   's=' . url_quote("{ \"shaders\" : [\"$id\"] }"),
                   'nt=1',
                   'nl=1',
                   'np=1');

  print STDERR "$progname: $id: loading $url\n" if ($verbose);
  my $body = load ($id, $url2, $post, $cookie);

  if (!$body || $body !~ m/^\[\{/s) {
    error ("$id: unable to load JSON$find");
  }

  my $json = json_decode ($body);
  error ("$id: unparsable JSON") unless $json;

  $json = $json->[0]              || error ("$id: unparsable JSON");
  my $info = $json->{info}        || error ("$id: JSON: no info");
  my $name = $info->{name}        || error ("$id: JSON: no name");
  my $user = $info->{username}    || error ("$id: JSON: no username");
  my $desc = $info->{description} || error ("$id: JSON: no description");
  my $date = $info->{date}        || error ("$id: JSON: no date");
  my $rend = $json->{renderpass}  || error ("$id: JSON: no renderpass");

  foreach ($name, $user, $desc) { s/^\s+|\s+$//gs; }

  # The "About" section of the /user/ page almost always has nothing on it.
  if (0) {
    my $url3 = "https://www.shadertoy.com/user/$user";
    my $body2 = load ($user, $url3, undef, $cookie);
    my ($about) = ($body2 =~ m@<b>About</b>(.*?)</td>@si);
    $about =~ s@<BR/?>|<P>@\n@gsi;
    $about =~ s/<[^<>]*>//gsi;
    $about =~ s/^\s+|\s+$//gsi;
    $about =~ s/\n\n+/\n/gsi;
  }

  $desc =~ s@\n@\n// @gs;
  $date = strftime ("%d-%b-%Y", localtime ($date));
  my $header = ("// Title:  $name\n" .
                "// Author: $user\n" .
                "// URL:    $url\n" .
                "// Date:   $date\n" .
                "// Desc:   $desc\n" .
                "\n");

  my $base = lc ($name);
  $base =~ s/[^a-z\d]+/-/gs;
  $base =~ s/-+/-/s;
  $base =~ s/^-+|-+$//s;

  my @common;
  my @passes;
  foreach my $pass (@$rend) {
    my $ptype = $pass->{type} || error ("$id: JSON: no pass type");
    my $code  = $pass->{code} || error ("$id: JSON: no pass code");
    $code = "$header$code\n";
    if ($ptype =~ m/common/si) {
      push @common, $code;
    } else {
      push @passes, $code;
    }
  }

  $base = "glsl/$base";

  my @out;
  push @out, [ "$base.json", $body, '' ];
  push @out, [ "$base-common.glsl", $common[0], '-common' ] if (@common);

  if (@passes == 1) {
    push @out, [ "$base.glsl", $passes[0], '' ];
  } else {
    my $i = 0;
    foreach my $c (@passes) {
      push @out, [ "$base-$i.glsl", $c, $i ];
      $i++;
    }
  }

  foreach my $P (@out) {
    my ($fn, $code, $pass) = @$P;
    if ($debug_p) {
      print STDERR "$progname: $id: not writing $fn\n";
    } else {
      open (my $out, '>:utf8', $fn) || error ("$id: $fn: $!");
      print $out $code;
      close ($out);
      print STDERR "$progname: $id: wrote $fn\n" if ($verbose);
    }
  }

  if ($verbose) {
    my $cmd = 'xshadertoy';
    my $i = 0;
    foreach my $P (@out) {
      my ($fn, $code, $pass) = @$P;
      next if ($fn =~ m/\.json$/s);
      $cmd .= " \\\n " if ($i > 0);
      $cmd .= " --program$pass $fn";
      $i++;
    }
    print STDERR "\n# $url\n$cmd\n";
  }
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose] [--quiet] [--cookie C] url...\n";
  exit 1;
}

sub main() {
  my @urls = ();
  my $cookie = '';
  while (@ARGV) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/s) { $verbose++; }
    elsif (m/^-v+$/s) { $verbose += length($_)-1; }
    elsif (m/^--?q(uiet)?$/s) { $verbose = 0; }
    elsif (m/^--?debug$/s) { $debug_p++; }
    elsif (m/^--?cookie$/s) { $cookie = shift @ARGV; }
    elsif (m/^-./s) { usage; }
    else { push @urls, $_; }
  }

  usage unless (@urls);
  foreach my $url (@urls) {
    download ($url, $cookie);
  }
}

main();
exit 0;
