#!/usr/bin/perl -w
# Copyright © 2006-2017 Jamie Zawinski <jwz@jwz.org>
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
#use diagnostics;	# Fails on some MacOS 10.5 systems
use strict;
use IPC::Open3;
use IO::Uncompress::Gunzip qw(gunzip $GunzipError);
use IO::Compress::Gzip qw(gzip $GzipError);

my ($exec_dir, $progname) = ($0 =~ m@^(.*?)/([^/]+)$@);

my ($version) = ('$Revision: 1.47 $' =~ m/\s(\d[.\d]+)\s/s);

$ENV{PATH} = "/usr/local/bin:$ENV{PATH}";   # for seticon
$ENV{PATH} = "/opt/local/bin:$ENV{PATH}";   # for macports wget

my $thumbdir = 'build/screenshots';



my $verbose = 1;

sub convert_plist($$) {
  my ($data, $to_binary_p) = @_;
  my $is_binary_p = ($data =~ m/^bplist/s);
  if ($data && (!$is_binary_p) != (!$to_binary_p)) {
    print STDERR "$progname: converting plist\n" if ($verbose > 2);
    my $which = ($to_binary_p ? 'binary1' : 'xml1');
    my @cmd = ('plutil', '-convert', $which, '-s', '-o', '-', '-');
    my $pid = open3 (my $in, my $out, undef, @cmd) ||
      error ("pipe: $cmd[0]: $!");
    error ("$cmd[0]: $!") unless $pid;
    print $in $data;
    close $in;
    local $/ = undef;  # read entire file
    $data = <$out>;
    close $out;
    waitpid ($pid, 0);
    if ($?) {
      my $exit_value  = $? >> 8;
      my $signal_num  = $? & 127;
      my $dumped_core = $? & 128;
      error ("$cmd[0]: core dumped!") if ($dumped_core);
      error ("$cmd[0]: signal $signal_num!") if ($signal_num);
      error ("$cmd[0]: exited with $exit_value!") if ($exit_value);
    }
  }
  return $data;
}


sub read_info_plist($) {
  my ($app_dir) = @_;
  my $file  = "$app_dir/Contents/Info.plist";
  my $file2 = "$app_dir/Info.plist";
  $file =~ s@/+@/@g;
  my $in;
  if (open ($in, '<', $file)) {
  } elsif (open ($in, '<', $file2)) {
    $file = $file2;
  } else {
    error ("$file: $!");
  }
  print STDERR "$progname: read $file\n" if ($verbose > 2);
  local $/ = undef;  # read entire file
  my $body = <$in>;
  close $in;

  $body = convert_plist ($body, 0);  # convert to xml plist
  return ($file, $body);
}


sub read_saver_xml($) {
  my ($app_dir) = @_;
  error ("$app_dir: no name") 
    unless ($app_dir =~ m@/([^/.]+).(app|saver)/?$@x);
  my $name  = $1;

  return () if ($name eq 'XScreenSaver');
  return () if ($name eq 'SaverTester');
  return () if ($name eq 'XScreenSaverUpdater');

  my $file  = "$app_dir/Contents/Resources/" . lc($name) . ".xml";
  my $file2 = "$app_dir/" . lc($name) . ".xml";
  my $file3 = "$app_dir/Contents/PlugIns/$name.saver/Contents/Resources/" .
              lc($name) . ".xml";
  $file =~ s@/+@/@g;
  my $in;
  if (open ($in, '<', $file)) {
  } elsif (open ($in, '<', $file2)) { $file = $file2;
  } elsif (open ($in, '<', $file3)) { $file = $file3;
  } else {
    error ("$file: $!");
  }
  print STDERR "$progname: read $file\n" if ($verbose > 2);
  local $/ = undef;  # read entire file
  my $body = <$in>;
  close $in;

  # Uncompress the XML if it is compressed.
  my $body2 = '';
  gunzip (\$body, \$body2) || error ("$app_dir: xml gunzip: $GunzipError");
  my $was_compressed_p = ($body ne $body2);
  return ($file, $body2, $was_compressed_p);
}


# This is duplicated in hacks/check-configs.pl for Android
#
sub munge_blurb($$$$) {
  my ($filename, $name, $vers, $desc) = @_;

  $desc =~ s/^([ \t]*\n)+//s;
  $desc =~ s/\s*$//s;

  # in case it's done already...
  $desc =~ s@<!--.*?-->@@gs;
  $desc =~ s/^.* version \d[^\n]*\n//s;
  $desc =~ s/^From the XScreenSaver.*\n//m;
  $desc =~ s@^https://www\.jwz\.org/xscreensaver.*\n@@m;
  $desc =~
       s/\nCopyright [^ \r\n\t]+ (\d{4})(-\d{4})? (.*)\.$/\nWritten $3; $1./s;
  $desc =~ s/^\n+//s;

  error ("$filename: description contains markup: $1")
    if ($desc =~ m/([<>&][^<>&\s]*)/s);
  error ("$filename: description contains ctl chars: $1")
    if ($desc =~ m/([\000-\010\013-\037])/s);

  error ("$filename: can't extract authors")
    unless ($desc =~ m@^(.*)\nWritten by[ \t]+(.+)$@s);
  $desc = $1;
  my $authors = $2;
  $desc =~ s/\s*$//s;

  my $year = undef;
  if ($authors =~ m@^(.*?)\s*[,;]\s+(\d\d\d\d)([-\s,;]+\d\d\d\d)*[.]?$@s) {
    $authors = $1;
    $year = $2;
  }

  error ("$filename: can't extract year") unless $year;
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
    if ($authors =~ m@^(.*?),? and (.*)$@s) {
      $authors = "$1, $2, and $curator";
    } else {
      $authors .= " and $curator";
    }
  }

  my $desc1 = ("$name, version $vers.\n\n" .		# savername.xml
               $desc . "\n" .
               "\n" . 
               "From the XScreenSaver collection: " .
               "https://www.jwz.org/xscreensaver/\n" .
               "Copyright \302\251 $year by $authors.\n");

  my $desc2 = ("$name $vers,\n" .			# Info.plist
               "\302\251 $year $authors.\n" .
               "From the XScreenSaver collection:\n" .
               "https://www.jwz.org/xscreensaver/\n" .
               "\n" .
               $desc .
               "\n");

  # unwrap lines, but only when it's obviously ok: leave blank lines,
  # and don't unwrap if that would compress leading whitespace on a line.
  #
  $desc2 =~ s/^(From |https?:)/\n$1/gm;
  1 while ($desc2 =~ s/([^\s])[ \t]*\n([^\s])/$1 $2/gs);
  $desc2 =~ s/\n\n(From |https?:)/\n$1/gs;

  return ($desc1, $desc2);
}


sub update_saver_xml($$) {
  my ($app_dir, $vers) = @_;
  my ($filename, $body, $was_compressed_p) = read_saver_xml ($app_dir);
  my $obody = $body;

  return () unless defined ($filename);

  $body =~ m@<screensaver[^<>]*?[ \t]_label=\"([^\"]+)\"@m ||
    error ("$filename: no name label");
  my $name = $1;

  $body =~ m@<_description>(.*?)</_description>@s ||
    error ("$filename: no description tag");
  my $desc = $1;

  error ("$filename: description contains non-ASCII and is not UTF-8: $1")
    if ($body !~ m/\Q<?xml version="1.0" encoding="UTF-8"/s &&
        $desc =~ m/([^\000-\176])/s);

  my ($desc1, $desc2) = munge_blurb ($filename, $name, $vers, $desc);

  $body =~ s@(<_description>)(.*?)(</_description>)@$1$desc1$3@s;

  # NSXMLParser doesn't seem to work properly on Latin1 XML documents,
  # so we convert these to UTF8 when embedding them in the .saver bundle.
  $body =~ s@encoding="ISO-8859-1"@encoding="UTF-8"@gsi;

  if ($obody eq $body && $was_compressed_p) {
    print STDERR "$progname: $filename: unchanged\n" if ($verbose > 1);
  } else {

    # Gzip the XML.
    my $body2 = '';
    gzip (\$body, \$body2) || error ("$app_dir: xml gzip: $GzipError");
    $body = $body2;

    my $file_tmp = "$filename.tmp";
    open (my $out, '>:raw', $file_tmp) || error ("$file_tmp: $!");
    print $out $body || error ("$file_tmp: $!");
    close $out || error ("$file_tmp: $!");

    if (!rename ("$file_tmp", "$filename")) {
      unlink "$file_tmp";
      error ("mv \"$file_tmp\" \"$filename\": $!");
    }
    print STDERR "$progname: wrote $filename\n" if ($verbose);
  }

  return ($desc1, $desc2);
}


sub compress_all_xml_files($) {
  my ($dir) = @_;
  opendir (my $dirp, $dir) || error ("$dir: $!");
  my @files = readdir ($dirp);
  closedir $dirp;
  foreach my $f (sort @files) {
    next unless ($f =~ m/\.xml$/si);
    my $filename = "$dir/$f";
    open (my $in, '<', $filename) || error ("$filename: $!");
    print STDERR "$progname: read $filename\n" if ($verbose > 2);
    local $/ = undef;  # read entire file
    my $body = <$in>;
    close $in;

    if ($body =~ m/^<\?xml/s) {
      my $body2 = '';
      gzip (\$body, \$body2) || error ("$filename: xml gzip: $GzipError");
      $body = $body2;
      my $file_tmp = "$filename.tmp";
      open (my $out, '>:raw', $file_tmp) || error ("$file_tmp: $!");
      print $out $body || error ("$file_tmp: $!");
      close $out || error ("$file_tmp: $!");

      if (!rename ("$file_tmp", "$filename")) {
        unlink "$file_tmp";
        error ("mv \"$file_tmp\" \"$filename\": $!");
      }
      print STDERR "$progname: compressed $filename\n" if ($verbose);
    } elsif ($verbose > 2) {
      print STDERR "$filename: already compressed\n";
    }
  }
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

  my $icon = ($app_dir =~ m/\.saver$/ ? 'XScreenSaver' : 'SaverRunner');
  $icon = "$app_dir/../../../$icon.icns";
  my @cmd = ("$app_dir/../../../seticon.pl", "-d", $icon, $app_dir);
  print STDERR "$progname: exec: " . join(' ', @cmd) . "\n"
    if ($verbose > 1);
  system (@cmd);
}


sub set_thumb($) {
  my ($app_dir) = @_;

  return unless ($app_dir =~ m@\.saver/?$@s);

  my $name = $app_dir;
  $name =~ s@^.*/@@s;
  $name =~ s@\..*?$@@s;
  $name = lc($name);

  $name = 'rd-bomb' if ($name eq 'rdbomb');  # sigh

  if (! -f "$thumbdir/$name.png") {
    system ("make", "$thumbdir/$name.png");
    my $exit  = $? >> 8;
    exit ($exit) if $exit;
    error ("unable to download $name.png")
      unless (-f "$thumbdir/$name.png");
  }

  $app_dir =~ s@/+$@@s;
  $app_dir .= "/Contents/Resources";
  error ("$app_dir does not exist") unless (-d $app_dir);

  system ("cp", "-p", "$thumbdir/$name.png", "$app_dir/thumbnail.png");
  my $exit  = $? >> 8;
  exit ($exit) if $exit;
}


sub enable_gc($) {
  my ($app_dir) = @_;

  return unless ($app_dir =~ m@\.saver/?$@s);
  my ($dir, $name) = ($app_dir =~ m@^(.*)/([^/]+)\.saver$@s);
  error ("unparsable: $app_dir") unless $name;
  my $exe = "$app_dir/Contents/MacOS/$name";
  my @cmd = ("$dir/enable_gc", $exe);
  print STDERR "$progname: exec: " . join(' ', @cmd) . "\n"
    if ($verbose > 1);
  system (@cmd);
  my $exit  = $? >> 8;
  exit ($exit) if $exit;
}


sub fix_coretext($) {
  my ($app_dir) = @_;

  # In MacOS 10.8, they moved CoreText.framework from
  # /System/Library/Frameworks/ApplicationServices.framework/Frameworks/
  # to /System/Library/Frameworks/ which means that executables compiled
  # on 10.8 and newer won't run on 10.7 and older because they can't find
  # the library. Fortunately, 10.8 and later leave a symlink behind, so
  # the old location still works. So we need our executables to contain
  # an LC_LOAD_DYLIB pointing at the old directory instead of the new
  # one.
  # 
  return if ($app_dir =~ m@-iphone@s);
  my ($dir, $name) = ($app_dir =~ m@^(.*)/([^/]+)\.(app|saver)$@s);
  error ("unparsable: $app_dir") unless $name;
  my $exe = "$app_dir/Contents/MacOS/$name";

  my $new = ("/System/Library/Frameworks/CoreText.framework/" .
             "Versions/A/CoreText");
  my $old = ("/System/Library/Frameworks/ApplicationServices.framework/" .
             "Frameworks/CoreText.framework/Versions/A/CoreText");
  my @cmd = ("install_name_tool", "-change", $new, $old, $exe);

  print STDERR "$progname: exec: " . join(' ', @cmd) . "\n"
    if ($verbose > 1);
  system (@cmd);
  my $exit  = $? >> 8;
  exit ($exit) if $exit;
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

  # No, don't do this -- the iOS version reads the XML file in a few
  # different places, and most of those places don't understand gzip.

  if ($app_name eq 'XScreenSaver') {
    compress_all_xml_files ($app_dir);
  } elsif (! defined($info_str)) {
    print STDERR "$progname: $filename: no XML file\n" if ($verbose > 1);
  } else {

    $info_str =~ m@^([^\n]+)\n@s ||
      error ("$filename: unparsable copyright");
    my $copyright = "$1";
    $copyright =~ s/\b\d{4}-(\d{4})\b/$1/;

    # Lose the Wikipedia URLs.
    $info_str =~ s@https?:.*?\b(wikipedia|mathworld)\b[^\s]+[ \t]*\n?@@gm;

    $info_str =~ s/(\n\n)\n+/$1/gs;
    $info_str =~ s/(^\s+|\s+$)//gs;
    $plist = set_plist_key ($filename, $plist, 
                            "NSHumanReadableCopyright", $copyright);
    $plist = set_plist_key ($filename, $plist,
                            "CFBundleLongVersionString",$copyright);
    $plist = set_plist_key ($filename, $plist,
                            "CFBundleGetInfoString",    $info_str);
    $plist = set_plist_key ($filename, $plist,
                            "CFBundleIdentifier",
                            "org.jwz.xscreensaver." . $app_name);

    if ($oplist eq $plist) {
      print STDERR "$progname: $filename: unchanged\n" if ($verbose > 1);
    } else {
      $plist = convert_plist ($plist, 1);  # convert to binary plist
      my $file_tmp = "$filename.tmp";
      open (my $out, '>:raw', $file_tmp) || error ("$file_tmp: $!");
      print $out $plist || error ("$file_tmp: $!");
      close $out || error ("$file_tmp: $!");

      if (!rename ("$file_tmp", "$filename")) {
        unlink "$file_tmp";
        error ("mv \"$file_tmp\" \"$filename\": $!");
      }
      print STDERR "$progname: wrote $filename\n" if ($verbose);
    }
  }

  # MacOS 10.12: codesign says "resource fork, Finder information, or
  # similar detritus not allowed" if any bundle has an Icon\r file.
  # set_icon ($app_dir);

  set_thumb ($app_dir);
# enable_gc ($app_dir);
  fix_coretext ($app_dir)
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
  while ($_ = $ARGV[0]) {
    shift @ARGV;
    if    (m/^--?verbose$/s)  { $verbose++; }
    elsif (m/^-v+$/)          { $verbose += length($_)-1; }
    elsif (m/^--?q(uiet)?$/s) { $verbose = 0; }
    elsif (m/^-/s)            { usage(); }
    else                      { push @files, $_; }
  }
  usage() unless ($#files >= 0);
  foreach (@files) {
    update ($_);
  }
}

main();
exit 0;
