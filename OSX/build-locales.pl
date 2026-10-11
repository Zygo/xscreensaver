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
# Convert Unix localizations to Cocoa localizations.
#
# Created:  9-Sep-2026.

require 5;
use diagnostics;
use strict;

use POSIX;
use IO::Uncompress::Gunzip qw(gunzip $GunzipError);

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.06 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;
my $debug_p = 0;


# Duplicated in po/autotranslate.pl and hacks/check-configs.pl
sub unquote_c_string($;$) {
  my ($str, $allow_octal_p) = @_;

  $str =~ s/\\\"/\001/gs;		   # Protect literal \"
  $str = join('', ($str =~ m/"(.*?)"/g));  # "ABC\n" "DEF\n" => "ABC\nDEF\n"
  $str =~ s/\001/\"/gs;

  $str =~ s/\\n/\n/gs;
  $str =~ s/\\t/\t/gs;
  $str =~ s/\\([-\"\'])/$1/gs;		   # Other backslashes

  if ($allow_octal_p) {
    $str =~ s@\\([0-7]{3})@{ chr(oct($1)) }@gsex;	# For countries.c
    utf8::decode ($str);  # Pack multi-byte UTF-8 back into wide chars.
  }

  error ("mystery backslashes: $str") if ($str =~ m/\\/s);
  return $str;
}


sub quote_c_string($) {
  my ($str) = @_;
  $str =~ s/\n/\\n/gs;		# quote \n
  $str =~ s/\t/\\t/gs;
  $str =~ s/([\"])/\\$1/gs;	# quote "
  $str = "\"$str\"";
  return $str;
}


sub safe_system(@) {
  my (@cmd) = @_;
  print STDERR "$progname: exec: " . join(' ', @cmd) . "\n" if ($verbose > 1);
  system @cmd;
  my $exit_value  = $? >> 8;
  my $signal_num  = $? & 127;
  my $dumped_core = $? & 128;
  error ("$cmd[0]: core dumped!") if ($dumped_core);
  error ("$cmd[0]: signal $signal_num!") if ($signal_num);
  error ("$cmd[0]: exit $exit_value") if ($exit_value);
  return $exit_value;
}


sub write_if_changed($$$) {
  my ($file, $body, $obody) = @_;

  if ($body eq $obody) {
    print STDERR "$progname: $file unchanged\n" if ($verbose || $debug_p);
    return;
  }

  if ($debug_p) {
    my $tmp1 = sprintf ("/tmp/bl-1-%08x", rand() * 0xFFFFFFFF);
    my $tmp2 = sprintf ("/tmp/bl-2-%08x", rand() * 0xFFFFFFFF);

    open (my $out1, '>:utf8', $tmp1) || error ("$tmp1: $!");
    print $out1 $obody;
    close $out1;

    open (my $out2, '>:utf8', $tmp2) || error ("$tmp2: $!");
    print $out2 $body;
    close $out2;

    print STDERR "$progname: not writing $file\n";
    system ("diff -U4 $tmp1 $tmp2");
    unlink ($tmp1);
    unlink ($tmp2);
  } else {
    open (my $out, '>:utf8', $file) || error ("$file: $!");
    print $out $body;
    close $out;
    print STDERR "$progname: wrote $file\n";
  }
}


sub read_saver_xml($) {
  my ($app_dir) = @_;

  error ("$app_dir: no name") 
    unless ($app_dir =~ m@/([^/.]+).(app|saver)/?$@x);
  my $name  = $1;

  $app_dir =~ s@/+$@@s;

  return () if ($name eq 'XScreenSaver');
  return () if ($name eq 'XScreenSaver-tvOS');
  return () if ($name eq 'SaverTester');
  return () if ($name eq 'XScreenSaverUpdater');
  return () if ($name eq 'RandomXScreenSaver');

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



sub localize($) {
  my ($app_dir) = @_;

  # We try to minimize the number of strings included to those actually
  # used by this hack, which means we have to parse out the source and XML
  # files to know which of the strings are actually used in this hack,
  # and only include those.

  error ("$app_dir: no name") 
    unless ($app_dir =~ m@/([^/.]+).(app|saver)/?$@x);
  my $app_name = $1;

  $app_dir =~ s@/+$@@gs;

  my $all_p = ($app_name eq 'XScreenSaver');

  return if ($app_name eq 'XScreenSaverUpdater');


  # Read po/LINGUAS for the list of languages.
  #
  my $body = '';
  my $file = '../po/LINGUAS';
  open (my $in, '<:utf8', $file) || error ("$file: $!");
  print STDERR "$progname: reading $file...\n" if ($verbose);
  local $/ = undef;  # read entire file
  while (<$in>) { $body .= $_; }
  close $in;

  $body =~ s/^\s+|\s+$//gs;
  my @all_langs = split (/\s+/, $body);


  # See if we have cached the localizations and if to use those instead.
  # This regenerates if the caches is more than 2 hours old, but it
  # doesn't manage any dependencies, so if strings have changed sooner,
  # rm build/xscreensaver.build/Debug*/*.build/*.strings
  # Might need to rm and make po/xscreensaver.pot as well.
  #
  my $res_dir = ($all_p
                 ? $app_dir
                 : $app_dir . '/Contents/Resources');
  my $cache_base = $ENV{'TEMP_FILES_DIR'};  # Xcode

  my $all_cached_p = defined ($cache_base);
  if (defined ($cache_base)) {
    foreach my $lang (@all_langs) {
    my $date = (stat("$cache_base/$app_name.$lang.strings"))[9];
      if (! defined($date) || $date < time() - (60*60)) {
        $all_cached_p = 0;
        last;
      }
    }
  }

  if ($all_cached_p) {
    foreach my $lang (@all_langs) {
      my $file1 = "$cache_base/$app_name.$lang.strings";
      my $dir   = "$res_dir/$lang.lproj";
      my $file2 = "$dir/Localizable.strings";
      if (!$debug_p && !-d $dir) {
        mkdir ($dir) || error ("$dir: $!");
      }
      if ($debug_p) {
        print STDERR "$progname: not writing $file2 (cached)\n";
      } else {
        safe_system ('cp', '-p', $file1, $file2);
        print STDERR "$progname: wrote $file2 (cached)\n";
      }
    }
    return;
  }


  # Read hacks/config/NAME.xml for this hack's UI strings.
  #
  my %keys;
  my %osx_keys;
  my $desc_key = undef;
  if ($app_name eq 'RandomXScreenSaver' ||
      $app_name eq 'SaverTester') {
  } elsif (! $all_p) {
    my ($filename, $body) = read_saver_xml ($app_dir);
    foreach my $key ($body =~ m@\s_[-a-z\d_]+\s*=\s*"(.*?)"@gsi) {
      $keys{$key} = $key;
    }

    $desc_key = '_' . lc($app_name) . '_description';
    my ($desc) = ($body =~ m@<_description[^<>]*>(.*?)</_description>@si);
    error ("$filename: no desc") unless $desc;
    $keys{$desc_key} = $desc;
  }

  # Read the source code for translatable strings in the config sheet, etc.
  #
  $body = '';
  my @srcs = ();
  foreach my $f (glob ('*.m')) {
    next if ($f eq 'Randomizer.m' && $app_name ne 'RandomXScreenSaver');
    push @srcs, $f;
  }

  if ($all_p) {
    push @srcs, glob ('../hacks/*.[ch]');
    push @srcs, glob ('../hacks/glx/*.[ch]');
  } else {
    push @srcs, glob ('../hacks/'     . lc($app_name) . '*.[ch]');
    push @srcs, glob ('../hacks/glx/' . lc($app_name) . '*.[ch]');
    push @srcs, '../hacks/glx/chessgames.h'   if ($app_name eq 'Endgame');
    push @srcs, '../hacks/glx/mapcities.h'    if ($app_name eq 'MapScroller');
    push @srcs, '../hacks/glx/polyhedra-gl.c' if ($app_name eq 'Polyhedra');
    push @srcs, '../hacks/glx/countries.c'    if ($app_name eq 'WorldPieces');
  }

  foreach my $file (@srcs) {
    open (my $in, '<:utf8', $file) || error ("$file: $!");
    print STDERR "$progname: reading $file...\n" if ($verbose);
    local $/ = undef;  # read entire file
    while (<$in>) { $body .= $_; }
    close $in;

    # _("A" "B")
    foreach my $key ($body =~ m/ _\( \s* ( \" .+? \" ) \s* \) /gsx) {
      $key = unquote_c_string ($key, 1);
      $keys{$key} = $key if ($key);
    }

    # NSLocalizedString (@"A" "B", ... )
    foreach my $key ($body =~ m/ \b NSLocalizedString \s* \( \s*
                                 \@ \s* ( \" .+? \" ) \s* , /gsx) {
      $key = unquote_c_string ($key);
      $keys{$key} = $key if ($key);
      $osx_keys{$key} = $key if ($key);
    }

    # @{ @"_low-label":  @"A" "B" }
    foreach my $key ($body =~ m/ \@ \s* "_[-_a-z]+" \s* : \s* 
                                 \@ \s* ( \" .*? \" ) \s* /gsx) {
      $key = unquote_c_string ($key);
      $osx_keys{$key} = $key if ($key);
    }
  }


  # Write ../po/osx.h so that the Unix side does the translating.
  # We only update this while building RandomXScreenSaver because
  # it has additional strings that the other savers omit.
  #
  if ($app_name eq 'RandomXScreenSaver') {
    $file = '../po/osx.h';
    $body = '';
    my $obody = '';
    if (open my $in, '<:utf8', $file) {
      local $/ = undef;  # read entire file
      while (<$in>) { $obody .= $_; }
      close $in;
    }
  
    # These are for Android. Hard to grep, so let's just hardcode them.
    #
    foreach my $key (# hacks/check-configs.pl
                     'Reset to defaults',
                     # android/xscreensaver/res/layout/activity_xscreensaver.xml
                     'Open live wallpaper list',
                     'Open screen saver list',
                     ) {
      $osx_keys{$key} = $key;
    }
  
    foreach my $key (sort { lc($a) cmp lc($b) } keys %osx_keys) {
      $body .= '_(' . quote_c_string ($key) . ")\n";
    }

    $body =
      ("/* Generated by $progname\n" .
       "\n" .
       "   These strings are used by the macOS, iOS and Android versions.\n" .
       "   This file exists to simplify the use of intltool to translate\n" .
       "   all of the various platforms' strings in one place.\n" .
       " */\n" .
       "\n" .
       $body);

    write_if_changed ($file, $body, $obody);
  }


  # Read each po/*.po for the translations.
  #
  foreach my $lang (@all_langs) {

    my $file = "../po/$lang.po";

    $body = '';
    open (my $in, '<:utf8', $file) || error ("$file: $!");
    print STDERR "$progname: reading $file...\n" if ($verbose);
    local $/ = undef;  # read entire file
    while (<$in>) { $body .= $_; }
    close $in;

    my %strings;
  
    $body =~ s/\\\"/\001/gs;  # Protect literal \"
    $body =~ s@( (?: \# [^\n]* \n )+ )
              msgid  \s+ ( (?: "[^\"]* " \s* )+ ) \s* \n
              msgstr \s+ ( (?: "[^\"]* " \s* )+ ) \s* \n? @{
      my ($comm, $msgid, $msgstr) = ($1, $2, $3);
  
      if ($comm =~ m/([^\/]+)\.xml/s) {
        my $hack = $1;
        if (($all_p || $hack eq lc($app_name)) &&
            $msgid =~ m/Written by/si) {
           # This is the <_description> in an XML file.  Set the key to the
           # hack name instead of the full string, because the strings in
           # the OSX XML files differ slightly from the Unix XML files.
           # (The hack name is prepended, and the authorship is tweaked.)
           $msgid = '"_' . $hack . '_description"';
        }
      }
  
      my $msgid2  = unquote_c_string ($msgid);
      my $msgstr2 = unquote_c_string ($msgstr);
  
      foreach ($comm, $msgid, $msgid2, $msgstr, $msgstr2) { s/\001/\\\"/gs; }
  
      if ($all_p || $keys{$msgid2}) {
        $strings{$msgid2} = $msgstr2 if ($msgid2 ne "");
      }

      "${comm}msgid $msgid\nmsgstr $msgstr\n";
    }@gsexi;

    $body .= ("\n\"" . $desc_key . '" = "' . $keys{$desc_key} . "\"\n\n")
      if ($desc_key);

    $body = '';
    $body = "/* Created by $progname */\n\n";
    foreach my $key (sort { my ($aa, $bb) = ($a, $b);
                            foreach ($aa, $bb) {
                              s/^(\s+|\\n|\\t)+//gs;
                            }
                            $aa cmp $bb;
                     } keys %strings) {
      my $val = $strings{$key};
      next unless $val;
      next if ($key eq $val);
      foreach ($key, $val) { s/\\//gs; }	# Oh FFS
      $body .= quote_c_string($key) . ' = ' . quote_c_string($val) . ";\n\n";
    }


    my $dir = ($all_p
               ? "$app_dir/$lang.lproj"
               : "$app_dir/Contents/Resources/$lang.lproj");
    $file = "$dir/Localizable.strings";

    my $obody = '';
    if (open my $in, '<:utf8', $file) {
      local $/ = undef;  # read entire file
      while (<$in>) { $obody .= $_; }
      close $in;
    }

    if (!$debug_p && !-d $dir) {
      my ($dir2) = ($dir =~ m@^(.*)/[^/]+/?$@s);
      if (!-d $dir2) {
        mkdir ($dir2) || error ("$dir2: $!");
      }
      mkdir ($dir) || error ("$dir: $!");
    }

    write_if_changed ($file, $body, $obody);

    if (defined ($cache_base)) {
      my $file2 = "$cache_base/$app_name.$lang.strings";
      if ($debug_p) {
        print STDERR "$progname: not caching $file2\n";
      } else {
        safe_system ('cp', '-p', $file, $file2);
        print STDERR "$progname: wrote $file2 (caching)\n";
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
  print STDERR "usage: $progname [--verbose] program.app ...\n";
  exit 1;
}

sub main() {
  binmode (STDOUT, ':utf8');
  binmode (STDERR, ':utf8');

  my @files = ();
  while (@ARGV) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/s) { $verbose++; }
    elsif (m/^-v+$/s) { $verbose += length($_)-1; }
    elsif (m/^--?debug$/s) { $debug_p++; }
    elsif (m/^-./s) { usage; }
    else { push @files, $_; }
#   else { usage; }
  }

  usage unless (@files);
  foreach (@files) { localize ($_); }
}

main();
exit 0;
