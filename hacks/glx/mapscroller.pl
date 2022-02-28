#!/usr/bin/perl -w
# Copyright © 2021-2022 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# This is forked by mapscroller.c to do network activity, since doing https
# from C code is untenable.
# 
# Created: 14-Dec-2021.

require 5;
#use diagnostics;	# Fails on some MacOS 10.5 systems
use strict;

use POSIX ('strftime', ':fcntl_h');		# S_ISDIR was here in Perl 5.6
import Fcntl ':mode' unless defined &S_ISUID;	# but it is here in Perl 5.8
	# but in Perl 5.10, both of these load, and cause errors!
	# So we have to check for S_ISUID instead of S_ISDIR?  WTF?
use Fcntl ':flock'; # import LOCK_* constants

# Some Linux systems don't install LWP by default!
BEGIN { eval 'use LWP::Simple' }

my $progname = $0; $progname =~ s@.*/@@g;
$progname =~ s@\.pl$@@g;
my ($version) = ('$Revision: 1.5 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;
my $url_template = undef;
my $cache_size = '20M';
my $cache_dir = undef;
my $current_cache_size = 0;
my %cached_files;

# Anything placed on this list gets unconditionally deleted when this
# script exits, even if abnormally.
#
my %rm_f;
END {
  my $exit = $?;
  my @rm_f = keys %rm_f;
  unlink @rm_f if (@rm_f);
  $? = $exit;  # Don't clobber this script's exit code.
}

sub signal_cleanup($) {
  my ($s) = @_;
  print STDERR "$progname: SIG$s\n" if ($verbose > 1);
  exit (1);  # This causes END{} to run.
}

$SIG{TERM} = \&signal_cleanup;  # kill
$SIG{INT}  = \&signal_cleanup;  # shell ^C
$SIG{QUIT} = \&signal_cleanup;  # shell ^|
$SIG{KILL} = \&signal_cleanup;  # nope
$SIG{ABRT} = \&signal_cleanup;
$SIG{PIPE} = \&signal_cleanup;
$SIG{HUP}  = \&signal_cleanup;


sub blurb() {
  return ("$progname: " . strftime('%H:%M:%S', localtime) .
          ': ' . getppid() . ': ');  # Parent, so multi-screen log lines match
}


sub init_lwp() {
  if (! defined ($LWP::Simple::ua)) {
    error ("\n\n\tPerl is broken. Do this to repair it:\n" .
           "\n\tsudo cpan LWP::Simple LWP::Protocol::https Mozilla::CA\n");
  }
}


my $sanity_checked_p = 0;
sub sanity_check_lwp() {
  return if $sanity_checked_p;
  $sanity_checked_p = 1;
  my $url1 = 'https://www.mozilla.org/';
  my $url2 =  'http://www.mozilla.org/';
  my $body = (LWP::Simple::get($url1) || '');
  if (length($body) < 10240) {
    my $err = "";
    $body = (LWP::Simple::get($url2) || '');
    if (length($body) < 10240) {
      $err = "Perl is broken: neither HTTP nor HTTPS URLs work.";
    } else {
      $err = "Perl is broken: HTTP URLs work but HTTPS URLs don't.";
    }
    $err .= "\nMaybe try: sudo cpan -f Mozilla::CA LWP::Protocol::https";
    $err =~ s/^/\t/gm;
    error ("\n\n$err\n");
  }
}


# Prevent multiple copies of this program from hitting the network at
# the same time, to be a little more gentle on the tile servers.
#
my $lock_fd = undef;  # global scope so it doesn't GC
sub acquire_lock() {
  my $lockfile = "$cache_dir/.read.lock";
  open ($lock_fd, '+>>', $lockfile) || error ("writing $lockfile: $!");

  while (1) {
    if (! flock ($lock_fd, LOCK_EX | LOCK_NB)) {
      if ($verbose > 3) {
        my $age = time() - (stat($lock_fd))[9];
        $age = sprintf("%d:%02d:%02d", $age/60/60, ($age/60)%60, $age%60);
        print STDERR "waiting for lock (locked for $age)\n";
      }
      select (undef, undef, undef, 0.1);  # Busy wait
    } else {
      print STDERR blurb() . "locked\n" if ($verbose > 2);
      # Acquired lock, set file mtime to now.
      # macOS 11.6, perl 5.28.3: "The futimes function is unimplemented".
      # This worked on macOS 10.14:
      # utime (undef, undef, $lock_fd);
      utime (undef, undef, $lockfile);
      last;
    }
  }
}

# Only one process is in charge of clearing the cache,
# and it holds that lock until exit and auto-unlock.
#
my $cache_lock_fd = undef;  # global scope so it doesn't GC
sub acquire_cache_lock() {

  return 1 if ($cache_lock_fd);  # We already hold the lock

  my $lockfile = "$cache_dir/.cache.lock";
  open ($cache_lock_fd, '+>>', $lockfile) || error ("writing $lockfile: $!");

  if (flock ($cache_lock_fd, LOCK_EX | LOCK_NB)) {
    print STDERR blurb() . "acquired cache lock\n" if ($verbose > 2);
    # Acquired lock, set file mtime to now.
    # macOS 11.6, perl 5.28.3: "The futimes function is unimplemented".
    # This worked on macOS 10.14:
    # utime (undef, undef, $cache_lock_fd);
    utime (undef, undef, $lockfile);
    return 1;
  } else {
    print STDERR blurb() . "cache locked by another process\n"
      if ($verbose > 2);
    close ($cache_lock_fd);
    $cache_lock_fd = undef;
    return 0;
  }
}


sub release_lock() {
  return unless $lock_fd;
  flock ($lock_fd, LOCK_UN) || error ("unable to unlock");
  $lock_fd = undef;
  print STDERR blurb() . "unlocked\n" if ($verbose > 2);
}


sub apply_template($$$) {
  my ($x, $y, $z) = @_;
  # https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
  my $url = $url_template;
  error ("bad url template: $url") unless ($url =~ m/\{/si);
  error ("no X in template: $url") unless ($url =~ m/\{\$?x/si);
  error ("no Y in template: $url") unless ($url =~ m/\{\$?y/si);
  error ("no Z in template: $url") unless ($url =~ m/\{\$?z/si);
  $url =~ s%\$?{(.*?)}%{
    my ($s) = $1;
    if    ($s =~ m/^\$?x$/si) { $s = $x; }       # {$x}
    elsif ($s =~ m/^\$?y$/si) { $s = $y; }       # {$y}
    elsif ($s =~ m/^\$?z$/si) { $s = $z; }       # {$z}
    elsif ($s =~ m/^([a-z\d])-([a-z\d])$/si) {   # {a-c}
      my ($from, $to) = (ord($1), ord($2));
      my @a = ( $from .. $to );
      $s = chr($a[rand() * @a]);
    } else { error ("unparsable substitution: {$s} in $url"); }
    $s;
  }%gsexi;

  error ("bad url template: $url") if ($url =~ m/[{}]/s);
  return $url;
}


# Returns cache file name, or 3-digit status code.
#
sub load_tile($$$$) {
  my ($ua, $x, $y, $z) = @_;

  my $timeout = 30;

  # Sanitize the template into a directory name
  my $site = lc($url_template);
  my ($suf) = ($site =~ m@\.([^./?#]+)$@si);
  error ("no suffix in url template: $url_template") unless $suf;

  $site =~ s@{.*?}@@gs;
  $site =~ s@\.[^./]+$@@gs;       # .png
  $site =~ s@^[^:/]+://@@gs;      # https://
  $site =~ s@^www\d*@@gs;         # www.
  $site =~ s@[^-_a-z\d.]@_@gs;    # non-alnum
  $site =~ s@([-_])\1+@$1@gs;     # consecutive _
  $site =~ s@^[-_.]+|[-_.]+$@@gs; # leading _
  error ("bad url template: $url_template") unless $site;

  # If the file already exists, just return it.
  #
  my $file = "$cache_dir/$site/$z/$y/$x.$suf";
  if (-f $file) {
    # Don't assume that the file exists because it is in $cached_files{$file}
    # as it might have been deleted when another process flushed the cache.
    print STDERR blurb() . "cached: $x $y $z\n" if ($verbose > 1);
    return $file;
  }

  my $url = apply_template ($x, $y, $z);

  # Hit the network.
  #
  my $tmpfile = sprintf("%s/%08x.%s", $cache_dir, rand() * 0xFFFFFFFF, $suf);
  $rm_f{$tmpfile} = 1;

  acquire_lock();
  print STDERR blurb() . "downloading $url\n" if ($verbose > 1);

  my $res = undef;
  $ua->timeout ($timeout);
  my $timed_out_p = 1;
  eval {
    local $SIG{ALRM} = sub { die "timed out\n"; };
    alarm $timeout;
    $res = $ua->get ($url, ':content_file' => $tmpfile);
    $timed_out_p = 0;
  };
  alarm 0;

  release_lock();

  if (!$res || !$res->is_success) {
    unlink $tmpfile;
    sanity_check_lwp();
    print STDERR blurb() . "$url " .
                 ($res ? $res->status_line : "timed out") . "\n"
      if ($verbose > 1);

    my $status = ($res && $res->code) || '';
    $status = '500' unless ($status =~ m/^\d\d\d$/si);
    $status = '500' if ($status eq '504');  # Ignore theirs
    $status = '504' if ($timed_out_p);      # It means only this
    return $status;
  }

  my $ct = $res->header ('Content-Type') || 'null';
  my $cl = $res->header ('Content-Length') || 0;
  if (! ($ct =~ m@^image/@si)) {
    unlink $tmpfile;
    sanity_check_lwp();
    print STDERR blurb() . "$url has content-type $ct\n" if ($verbose > 1);
    return '400';  # Let's call it "Bad Request" and not retry.
  }

  if ($cl < 10) {     # Ocean tiles are only 103 bytes!
    unlink $tmpfile;
    sanity_check_lwp();
    print STDERR blurb() . "$url has content-length $cl\n" if ($verbose > 1);
    return '400';  # Let's call it "Bad Request" and not retry.
  }

  # We got the document, and it has a sensible content type.
  # Move it into place.

  # Make sure the subdirs exist.
  foreach my $d ("$cache_dir/$site",
                 "$cache_dir/$site/$z",
                 "$cache_dir/$site/$z/$y") {
    if (! -d $d) {
      mkdir ($d) || error ("mkdir $d: $!");
      print STDERR blurb() . "mkdir $d\n" if ($verbose > 2);
    }
  }

  utime (undef, undef, $tmpfile);
  rename ($tmpfile, $file) || error ("mv $tmpfile $file: $!");
  delete $rm_f{$tmpfile};
  $current_cache_size += $cl;
  $cached_files{$file} = time();
  return $file;
}


# To keep the cache below a fixed size, we need to tally up the sizes
# of every file in the directory.  And to delete them by least-recently-used,
# we need to retain the last-access date of each file.  This could be a lot
# of RAM, I suppose.
#
# If two copies of this are running, only one of them is in charge of the
# cache.  Should that other process die, the remaining process will take
# over the lock.
#
sub scan_cache_dir($);
sub scan_cache_dir($) {
  my ($dir) = @_;
  opendir (my $dh, $dir) || error ("$dir: $!");
  print STDERR blurb() . "scanning $dir\n" if ($verbose > 3);
  foreach my $f (readdir ($dh)) {
    next if ($f =~ m/^\./si);
    $f = "$dir/$f";
    my @st = stat($f);
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks) = @st;
    if (S_ISDIR($mode)) {
      scan_cache_dir ($f);
    } else {
      $cached_files{$f} = $atime;
      $current_cache_size += $size;
    }
  }
  closedir $dh;
}


sub clean_cache() {
  return if ($current_cache_size <= $cache_size);
  my $osize = $current_cache_size;
  my $now = time();
  my $deleted_bytes = 0;
  my $deleted_files = 0;

  # Delete a bit more than necessary so that we re-scan less frequently.
  # 2MB means re-scanning roughly every 20-40 minutes, with one screen.
  my $slack = $cache_size * 0.5;
  $slack = 2*1024*1024 if ($slack > 2*1024*1024);

  foreach my $f (sort { $cached_files{$a} <=> $cached_files{$b} }
                 keys %cached_files) {

    last if ($current_cache_size <= $cache_size - $slack);

    my @st = stat($f);
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks) = @st;
    next unless defined ($size);    # File aready missing
    next if ($mtime >= $now - 60);  # Keep file if less than a minute old
    unlink ($f); # || error ("rm $f: $!");
    print STDERR blurb() . "rm $f (" . localtime($cached_files{$f}) . ")\n"
      if ($verbose > 2);
    delete $cached_files{$f};
    $current_cache_size -= $size;
    $deleted_bytes += $size;
    $deleted_files++;

    # Also delete now-empty subdirectories.
    my $depth = 0;
    while ($depth < 3 && $f =~ s@/[^/]+$@@) {
      $depth++;
      if (rmdir ($f)) {
        print STDERR blurb() . "rmdir $f\n" if ($verbose > 2);
      } else {
        print STDERR blurb() . "rmdir $f: not empty\n" if ($verbose > 3);
        last;
      }
    }
  }

  if ($deleted_files && $verbose) {
    my $b  = sprintf ("%.1f MB", $deleted_bytes / (1024*1024));
    my $b2 = sprintf ("%.1f MB", $osize         / (1024*1024));
    print STDERR blurb() . "deleted $deleted_files cached tiles" .
                 " ($b of $b2)\n";
  }
}


sub mapscroller() {
  init_lwp();
  my $ua = $LWP::Simple::ua;
  my $name = "xscreensaver-$progname";
  $name =~ s/\.pl$//s;
  $ua->agent ("$name/$version");

  my $scanned_p = 0;
  my $last_cleaned = time();
  my $clean_every = 45;

  while (<STDIN>) {
    my $line = $_;
    my ($x, $y, $z) = ($line =~ m/^(\d+)\s+(\d+)\s+(\d+)\s*$/s);
    error ("unparsable: $line") unless defined ($z);
    my $file = load_tile ($ua, $x, $y, $z);
    print STDOUT "$x $y $z\t$file\n" if defined ($file);

    # Don't prune the cache more often than every N seconds.
    # And don't do it on the first run, as we are probably loading
    # a burst of tiles.
    if (time() > $last_cleaned + $clean_every) {
      if (acquire_cache_lock()) {
        if (! $scanned_p) {
          $scanned_p = 1;
          undef %cached_files;	# Clear it, as scan is definitive.
          $current_cache_size = 0;
          scan_cache_dir ($cache_dir);
          if ($verbose) {
            my $n  = scalar (keys %cached_files);
            my $b1 = sprintf ("%.1f MB", $current_cache_size / (1024*1024));
            my $b2 = sprintf ("%.0f MB", $cache_size         / (1024*1024));
            print STDERR blurb() . "$n cached tiles ($b1 of $b2)\n";
          }
        }
        clean_cache();
      }
      $last_cleaned = time();
    }
  }
}


sub error($) {
  my ($err) = @_;
  print STDERR blurb() . "$err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname --url-template \"U\" " .
    "[--cache-size \"N MB\"] [--verbose]\n" .
    "\n" .
    "This is a helper program for the 'mapscroller' screen saver.\n" .
    "Reads \"X Y Z\" on stdin and writes cached tile filenames to stdout.\n" .
    "\n";
  exit 1;
}

sub main() {

  $|=1;
  binmode (STDIN,  ':utf8');
  binmode (STDOUT, ':utf8');
  binmode (STDERR, ':utf8');

  while (@ARGV) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/s) { $verbose++; }
    elsif (m/^-v+$/s) { $verbose += length($_)-1; }
    elsif (m/^--?url(-template)$/s) { $url_template = shift @ARGV; }
    elsif (m/^--?cache-size$/s)     { $cache_size   = shift @ARGV; }
    elsif (m/^-./s) { usage; }
    else { usage; }
  }

  usage unless (($url_template || '') =~ m@^https?://@s);

  my $s = $cache_size;
  if    ($s =~ s@\s*KB?$@@si) { $s = $s * 1024; }
  elsif ($s =~ s@\s*MB?$@@si) { $s = $s * 1024*1024; }
  elsif ($s =~ s@\s*GB?$@@si) { $s = $s * 1024*1024*1024; }
  elsif ($s =~ m/^\d+$/s)     { $s = 0 + $s; }
  else { error ("unparsable units: $s"); }
  $cache_size = $s;

  my $dd = "$ENV{HOME}/Library/Caches";    # MacOS location
  if (-d $dd) {
    $cache_dir = "$dd/org.jwz.xscreensaver.mapscroller";
  } elsif (-d "$ENV{HOME}/.cache") {	   # Gnome "FreeDesktop XDG" location
    $dd = "$ENV{HOME}/.cache/xscreensaver";
    if (! -d $dd) { mkdir ($dd) || error ("mkdir $dd: $!"); }
    $cache_dir = "$dd/mapscroller"
  } elsif (-d "$ENV{HOME}/tmp") {	   # If ~/tmp/ exists, use it.
    $cache_dir = "$ENV{HOME}/tmp/.xscreensaver-mapscroller";
  } else {
    $cache_dir = "$ENV{HOME}/.xscreensaver-mapscroller.cache";
  }

  $dd = $cache_dir;
  if (! -d $dd) { mkdir ($dd) || error ("mkdir $dd: $!"); }

  mapscroller();
}

main();
exit 0;
