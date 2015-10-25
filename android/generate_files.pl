#!/usr/bin/perl -w
# Copyright © 2008-2015 Jamie Zawinski <jwz@jwz.org>
# Copyright © 2015 Dennis Sheil <dennis@panaceasupplies.com>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# This parses the .c and .xml files and makes sure they are in sync: that
# options are spelled the same, and that all the numbers are in sync.
# Some of that functionality may be removed in the future.
#
# This also generates necessary Android files based on the information in
# those source and XML files.
#
# For the moment, the get_keys_and_values() subroutine is where support for
# previously unsupported Android live wallpapers is added.
#
# Created:  13-May-2015.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.1 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;


my $xlockmore_default_opts = '';
foreach (qw(count cycles delay ncolors size font)) {
  $xlockmore_default_opts .= "{\"-$_\", \".$_\", XrmoptionSepArg, 0},\n";
}
$xlockmore_default_opts .= 
 "{\"-wireframe\", \".wireframe\", XrmoptionNoArg, \"true\"},\n" .
 "{\"-3d\", \".use3d\", XrmoptionNoArg, \"true\"},\n" .
 "{\"-no-3d\", \".use3d\", XrmoptionNoArg, \"false\"},\n";

my $thread_default_opts = 
  "{\"-threads\",    \".useThreads\", XrmoptionNoArg, \"True\"},\n" .
  "{\"-no-threads\", \".useThreads\", XrmoptionNoArg, \"False\"},\n";

my $analogtv_default_opts = '';
foreach (qw(color tint brightness contrast)) {
  $analogtv_default_opts .= "{\"-tv-$_\", \".TV$_\", XrmoptionSepArg, 0},\n";
}

$analogtv_default_opts .= $thread_default_opts;


sub parse_settings_xml($) {

  my ($saver) = @_;

  my $file = "project/xscreensaver/res/values/settings.xml";
  my $body = '';

  local *IN;

  if (-e $file) {
      open (IN, '<', $file) || error ("$file: $!");
  }
  else {
      my @short;
      return @short;
  }

  while (<IN>) { $body .= $_; }
  close IN;
  $file =~ s@^.*/@@;
  $body =~ s/<!--.*?-->/ /gsi;
  $body =~ s/\s+/ /gs;
  $body =~ s/</\001</gs;

  my (@all);
  my $loop;

  foreach (split (m/\001/, $body)) {
    next if (m/^\s*$/s);
    my ($type, $args) = m@^<([?/]?[-_a-z]+)\b\s*(.*)$@si;
    error ("$progname: $file: unparsable: $_") unless $type;
    next if ($type =~ m@^/@);

    if ($type =~ m/^(\?xml|resources)/s) {

    } elsif ($type eq 'string-array') {
      my ($name) = ($args =~ m/\bname\s*=\s*\"([^\"]+)\"/);
      my ($value) = ($args =~ m/>([^\"]+)/);
      $loop = $name;

      if ($name =~ /^$saver/) {
        error ("$saver: $saver already in $file");
      }

    } elsif ($type eq 'item') {

      my ($item_value) = ($args =~ m/>(.+)/);
      my $item = $loop . " = " . $item_value;
      push @all, $item;

    } else {
      error ("$file: unknown type \"$type\" for no arg");
    }
  }

  return @all;

}


sub parse_items_xml($) {

  my ($saver) = @_;

  my $file = "project/xscreensaver/res/values/items.xml";
  my $body = '';
  my (%pixkeys) ;

  local *IN;

  if (-e $file) {
      open (IN, '<', $file) || error ("$file: $!");
  }
  else {
      my %short;
      return %short;
  }

  while (<IN>) { $body .= $_; }
  close IN;
  $file =~ s@^.*/@@;
  $body =~ s/<!--.*?-->/ /gsi;

  $body =~ s/\s+/ /gs;
  $body =~ s/</\001</gs;

  foreach (split (m/\001/, $body)) {
    next if (m/^\s*$/s);
    my ($type, $args) = m@^<([?/]?[-_a-z]+)\b\s*(.*)$@si;
    error ("$progname: $file: unparsable: $_") unless $type;
    next if ($type =~ m@^/@);

    if ($type =~ m/^(\?xml|resources)/s) {

    } elsif ($type eq 'item') {
      my ($name) = ($args =~ m/\bname\s*=\s*\"([^\"]+)\"/);
      my ($value) = ($args =~ m/>([^\"]+)/);

      if ($name =~ /^$saver/) {
        error ("$saver: $saver already in $file");
      }

      $pixkeys{$name} = $value;

    } else {
      error ("$file: unknown type \"$type\" for no arg");
    }
  }

  return (%pixkeys);
}


sub parse_glue($) {
  my ($saver) = @_;
  my $file = "gen/glue.c";
  my $in;

  if (-e $file) {
      open ($in, '<', $file) || error ("$file: $!");
  }
  else {
      my @short;
      return @short;
  }

  my $body = '';
  while (<$in>) { $body .= $_; }
  close $in;
  $file =~ s@^.*/@@;
  $body =~ s@^#\s*(if|ifdef|ifndef|elif|else|endif).*$@@gm;

  my (@hacks);
  if ($body =~ m/table\s*\*([a-z,\s\*_]+)xscreensaver_function_table;/s) {
    foreach (split (/,\s*\n/, $1)) {
      s/^\s*//s;
      s/\*//s;
      my @ftables = split (/_/, $_);
      my $ftable = $ftables[0];
      if ($ftable eq $saver) {
         error("$saver is already in glue");
      }
      push @hacks, $ftable;
    }
  }
  return @hacks;
}

# Returns two tables:
# - A table of the default resource values.
# - A table of "-switch" => "resource: value", or "-switch" => "resource: %"
#
sub parse_src($) {
  my ($saver) = @_;
  my $ffile = lc($saver) . ".c";

  # kludge...
  $ffile = 'apple2-main.c' if ($ffile eq 'apple2.c');
  $ffile = 'sproingiewrap.c' if ($ffile eq 'sproingies.c');
  $ffile = 'b_lockglue.c' if ($ffile eq 'bubble3d.c');
  $ffile = 'polyhedra-gl.c' if ($ffile eq 'polyhedra.c');
  $ffile = 'companion.c' if ($ffile eq 'companioncube.c');

  my $file = "../hacks/" . $ffile;

  $file = "../hacks/glx/$ffile" unless (-f $file);
  my $body = '';
  open (my $in, '<', $file) || error ("$file: $!");
  while (<$in>) { $body .= $_; }
  close $in;
  $file =~ s@^.*/@@;

  my $xlockmore_p = 0;
  my $thread_p = ($body =~ m/THREAD_DEFAULTS/);
  my $analogtv_p = ($body =~ m/ANALOGTV_DEFAULTS/);

  $body =~ s@/\*.*?\*/@@gs;
  $body =~ s@^#\s*(if|ifdef|ifndef|elif|else|endif).*$@@gm;
  $body =~ s/(THREAD|ANALOGTV)_(DEFAULTS|OPTIONS)(_XLOCK)?//gs;

  print STDERR "$progname: $file: defaults:\n" if ($verbose > 2);
  my %res_to_val;
  if ($body =~ m/_defaults\s*\[\]\s*=\s*{(.*?)}\s*;/s) {
    foreach (split (/,\s*\n/, $1)) {
      s/^\s*//s;
      s/\s*$//s;
      next if m/^0?$/s;
      my ($key, $val) = m@^\"([^:\s]+)\s*:\s*(.*?)\s*\"$@;
      print STDERR "$progname: $file: unparsable: $_\n" unless $key;
      $key =~ s/^[.*]//s;
      $res_to_val{$key} = $val;
      print STDERR "$progname: $file:   $key = $val\n" if ($verbose > 2);
    }
  } elsif ($body =~ m/\#\s*define\s*DEFAULTS\s*\\?\s*(.*?)\n[\n#]/s) {
    $xlockmore_p = 1;
    my $str = $1;
    $str =~ s/\"\s*\\\n\s*\"//gs;
    $str =~ m/^\s*\"(.*?)\"\s*\\?\s*$/ || 
      error ("$file: unparsable defaults: $str");
    $str = $1;
    $str =~ s/\s*\\n\s*/\n/gs;
    foreach (split (/\n/, $str)) {
      my ($key, $val) = m@^([^:\s]+)\s*:\s*(.*?)\s*$@;
      print STDERR "$progname: $file: unparsable: $_\n" unless $key;
      $key =~ s/^[.*]//s;
      $res_to_val{$key} = $val;
      print STDERR "$progname: $file:   $key = $val\n" if ($verbose > 2);
    }

    while ($body =~ s/^#\s*define\s+(DEF_([A-Z\d_]+))\s+\"([^\"]+)\"//m) {
      my ($key1, $key2, $val) = ($1, lc($2), $3);
      $key2 =~ s/_(.)/\U$1/gs;  # "foo_bar" -> "fooBar"
      $key2 =~ s/Rpm/RPM/;      # kludge
      $res_to_val{$key2} = $val;
      print STDERR "$progname: $file:   $key1 ($key2) = $val\n" 
        if ($verbose > 2);
    }

  } else {
    error ("$file: no defaults");
  }

  $body =~ m/XSCREENSAVER_MODULE(_2)?\s*\(\s*\"([^\"]+)\"/ ||
    error ("$file: no module name");
  $res_to_val{progclass} = $2;
  $res_to_val{doFPS} = 'false';
  print STDERR "$progname: $file:   progclass = $2\n" if ($verbose > 2);

  print STDERR "$progname: $file: switches to resources:\n"
    if ($verbose > 2);
  my %switch_to_res;
  $switch_to_res{-fps} = 'doFPS: true';
  $switch_to_res{-fg}  = 'foreground: %';
  $switch_to_res{-bg}  = 'background: %';

  my ($ign, $opts) = ($body =~ m/(_options|\bopts)\s*\[\]\s*=\s*{(.*?)}\s*;/s);
  if  ($xlockmore_p || $thread_p || $analogtv_p || $opts) {
    $opts = '' unless $opts;
    $opts .= ",\n$xlockmore_default_opts" if ($xlockmore_p);
    $opts .= ",\n$thread_default_opts" if ($thread_p);
    $opts .= ",\n$analogtv_default_opts" if ($analogtv_p);

    foreach (split (/,\s*\n/, $opts)) {
      s/^\s*//s;
      s/\s*$//s;
      next if m/^$/s;
      next if m/^{\s*0\s*,/s;
      my ($switch, $res, $type, $v0, $v1, $v2) =
        m@^ \s* { \s * \"([^\"]+)\" \s* ,
                  \s * \"([^\"]+)\" \s* ,
                  \s * ([^\s]+)     \s* ,
                  \s * (\"([^\"]*)\"|([a-zA-Z\d_]+)) \s* }@xi;
      print STDERR "$progname: $file: unparsable: $_\n" unless $switch;
      my $val = defined($v1) ? $v1 : $v2;
      $val = '%' if ($type eq 'XrmoptionSepArg');
      $res =~ s/^[.*]//s;
      $res =~ s/^[a-z\d]+\.//si;
      $switch =~ s/^\+/-no-/s;

      $val = "$res: $val";
      if (defined ($switch_to_res{$switch})) {
        print STDERR "$progname: $file:   DUP! $switch = \"$val\"\n" 
          if ($verbose > 2);
      } else {
        $switch_to_res{$switch} = $val;
        print STDERR "$progname: $file:   $switch = \"$val\"\n" 
          if ($verbose > 2);
      }
    }
  } else {
    error ("$file: no options");
  }

  return (\%res_to_val, \%switch_to_res);
}

# Returns a list of:
#    "resource = default value"
# or "resource != non-default value"
#
sub parse_manifest_xml($$) {
  my @result = ();
  my ($saver, $switch_to_res) = @_;
  my $file = "project/xscreensaver/AndroidManifest.xml";
  my $body = '';
  local *IN;

  if (-e $file) {
      open (IN, "<$file") || error ("$file: $!");
  }
  else {
      return @result;
  }

  while (<IN>) { $body .= $_; }
  close IN;
  $file =~ s@^.*/@@;

  $body =~ s/<!--.*?-->/ /gsi;

  $body =~ s/\s+/ /gs;
  $body =~ s/</\001</gs;
  $body =~ s/\001(<option)/$1/gs;

  print STDERR "$progname: $file: options:\n" if ($verbose > 2);

  foreach (split (m/\001/, $body)) {
    next if (m/^\s*$/s);
    my ($type, $args) = m@^<([?/]?[-_a-z]+)\b\s*(.*)$@si;
    error ("$progname: $file: unparsable: $_") unless $type;
    next if ($type =~ m@^/@);
    if ($type eq 'meta-data') {
        my ($value) = ($args =~ m/\@xml\/([^\"]+)\"/);
        push @result, $value;
    }
  }
  return @result;
}

# Returns a list of:
#    "resource = default value"
# or "resource != non-default value"
#
sub parse_strings_xml($$) {
  my @result = ();
  my ($saver, $switch_to_res) = @_;
  my $file = "project/xscreensaver/res/values/strings.xml";
  my $body = '';
  local *IN;

  if (-e $file) {
      open (IN, "<$file") || error ("$file: $!");
  }
  else {
      return @result;
  }

  while (<IN>) { $body .= $_; }
  close IN;
  $file =~ s@^.*/@@;

  $body =~ s/<!--.*?-->/ /gsi;

  $body =~ s/\s+/ /gs;
  $body =~ s/</\001</gs;
  $body =~ s/\001(<option)/$1/gs;

  print STDERR "$progname: $file: options:\n" if ($verbose > 2);

  my $saver_name = $saver . "_name";

  foreach (split (m/\001/, $body)) {
    next if (m/^\s*$/s);
    my ($type, $args) = m@^<([?/]?[-_a-z]+)\b\s*(.*)$@si;
    error ("$progname: $file: unparsable: $_") unless $type;
    next if ($type =~ m@^/@);

    if ($type =~ m/^([hv]group|\?xml|resources|xscreensaver-(image|text|updater))/s) {

    } elsif ($type eq 'string') {
      my ($name) = ($args =~ m/\bname\s*=\s*\"([^\"]+)\"/);
      my ($value) = ($args =~ m/>([^\"]+)/);
      my ($val) = "$name = $value";
      if ($saver_name eq $name) {
        error ("$saver: $saver already in $file");
      }
      push @result, $val;
    } elsif ($type eq 'item')  {
      # ignore
    } else {
      error ("$file: unknown type \"$type\" for no arg");
    }
  }

  return @result;
}



# Returns a list of:
#    "resource = default value"
# or "resource != non-default value"
#
sub parse_xml($$) {
  my ($saver, $switch_to_res) = @_;
  my $file = "../hacks/config/" . lc($saver) . ".xml";
  my $body = '';
  local *IN;
  open (IN, "<$file") || error ("$file: $!");
  while (<IN>) { $body .= $_; }
  close IN;
  $file =~ s@^.*/@@;

  my @result = ();

  $body =~ s/<!--.*?-->/ /gsi;

  $body =~ s/\s+/ /gs;
  $body =~ s/</\001</gs;
  $body =~ s/\001(<option)/$1/gs;

  my $video = undef;

  print STDERR "$progname: $file: options:\n" if ($verbose > 2);
  foreach (split (m/\001/, $body)) {
    next if (m/^\s*$/s);
    my ($type, $args) = m@^<([?/]?[-_a-z]+)\b\s*(.*)$@si;

    my $type_val;
    error ("$progname: $file: unparsable: $_") unless $type;
    next if ($type =~ m@^/@);

    if ($type =~ m/^([hv]group|\?xml|command|string|file|_description|xscreensaver-(image|text|updater))/s) {

    } elsif ($type eq 'screensaver') {
      my ($name) = ($args =~ m/\b_label\s*=\s*\"([^\"]+)\"/);
      my $val = "progclass = $name";
      push @result, $val;
      print STDERR "$progname: $file:   name:    $name\n" if ($verbose > 2);

    } elsif ($type eq 'video') {
      error ("$file: multiple videos") if $video;
      ($video) = ($args =~ m/\bhref="(.*?)"/);
      error ("$file: unparsable video") unless $video;
      error ("$file: unparsable video URL")
        unless ($video =~ m@^https?://www\.youtube\.com/watch\?v=[^?&]+$@s);

    } elsif ($type eq 'number') {
      my ($arg) = ($args =~ m/\barg\s*=\s*\"([^\"]+)\"/);
      my ($val) = ($args =~ m/\bdefault\s*=\s*\"([^\"]+)\"/);
      $val = '' unless defined($val);

      my ($low) = ($args =~ m/\blow\s*=\s*\"([^\"]+)\"/);
      my ($high) = ($args =~ m/\bhigh\s*=\s*\"([^\"]+)\"/);

      my ($ll) = ($args =~ m/\b_low-label\s*=\s*\"([^\"]+)\"/);
      my ($hl) = ($args =~ m/\b_high-label\s*=\s*\"([^\"]+)\"/);

      my $switch = $arg;
      $switch =~ s/\s+.*$//;
      my ($res) = $switch_to_res->{$switch};
      error ("$file: no resource for $type switch \"$arg\"") unless $res;
      $res =~ s/: \%$//;
      error ("$file: unparsable value: $res") if ($res =~ m/:/);

      $type_val = "$res" . "_type = $type";
      push @result, $type_val;
      $val = "$res = $val";
      push @result, $val;
      $val = "$res" . "_low = $low";
      push @result, $val;
      $val = "$res" . "_high = $high";
      push @result, $val;
      $val = "$res" . "_low-label = $ll";
      push @result, $val;
      $val = "$res" . "_high-label = $hl";
      push @result, $val;

      print STDERR "$progname: $file:   number:  $val\n" if ($verbose > 2);

    } elsif ($type eq 'boolean') {
      my ($set)   = ($args =~ m/\barg-set\s*=\s*\"([^\"]+)\"/);
      my ($unset) = ($args =~ m/\barg-unset\s*=\s*\"([^\"]+)\"/);
      my ($arg) = $set || $unset || error ("$file: unparsable: $args");
      my ($res) = $switch_to_res->{$arg};
        error ("$file: no resource for boolean switch \"$arg\"") unless $res;
      my ($res2, $val) = ($res =~ m/^(.*?): (.*)$/s);
      error ("$file: unparsable boolean resource: $res") unless $res2;
      $res = $res2;
      $type_val = "$res" . "_type = $type";
      push @result, $type_val;
#      $val = ($set ? "$res != $val" : "$res = $val");
      $val = "$res != $val";
      push @result, $val;

      print STDERR "$progname: $file:   boolean: $val\n" if ($verbose > 2);

    } elsif ($type eq 'select') {
      $args =~ s/</\001</gs;
      my @opts = split (/\001/, $args);
      shift @opts;
      my $unset_p = 0;
      my $this_res = undef;
      foreach (@opts) {
        error ("$file: unparsable: $_") unless (m/^<option\s/);
        my ($set) = m/\barg-set\s*=\s*\"([^\"]+)\"/;
        if ($set) {
          my ($set2, $val) = ($set =~ m/^(.*?) (.*)$/s);
          $set = $set2 if ($set2);
          my ($res) = $switch_to_res->{$set};
          error ("$file: no resource for select switch \"$set\"") unless $res;

          my ($res2, $val2) = ($res =~ m/^(.*?): (.*)$/s);
          error ("$file: unparsable select resource: $res") unless $res2;
          $res = $res2;
          $type_val = "$res" . "_type = $type";
          push @result, $type_val;
          $val = $val2 unless ($val2 eq '%');

          error ("$file: mismatched resources: $res vs $this_res")
            if (defined($this_res) && $this_res ne $res);
          $this_res = $res;

          $val = "$res != $val";
          push @result, $val;
          $val = "$res" . "_type = $type";
          push @result, $val;

          print STDERR "$progname: $file:   select:  $val\n" if ($verbose > 2);

        } else {
          error ("$file: multiple default options: $set") if ($unset_p);
          $unset_p++;
        }
      }

    } else {
      error ("$file: unknown type \"$type\" for no arg");
    }
  }

#  error ("$file: no video") unless $video;
  print STDERR "\n$file: WARNING: no video\n\n" unless $video;

  return @result;
}


sub parse_then_make($) {
  my ($saver) = @_;

  # kludge
  return 0 if ($saver =~ m/(-helper)$/);

  my ($src_opts, $switchmap) = parse_src ($saver);
  my (@xml_opts) = parse_xml ($saver, $switchmap);

  # tests if hack is supported yet
  my (@test) = get_keys_and_values($saver, @xml_opts);
  my (@strings_xml_opts) = parse_strings_xml ($saver, $switchmap);
  my (%pixkeys) =  parse_items_xml($saver);
  my (@manifest_xml_opts) = parse_manifest_xml ($saver, $switchmap);
  my (@glue_hacks) = parse_glue($saver);
  my (@settings_xml_opts) = parse_settings_xml($saver);

  my (@all_settings) = get_settings($saver, $switchmap, \@xml_opts);

  make_settings($saver);
  make_service($saver);
  make_wallpaper($saver, @xml_opts);

  make_manifest($saver, @manifest_xml_opts);

  make_hack_xml($saver);
  make_hack_settings_xml($saver, @xml_opts);
  make_strings_xml($saver, \@xml_opts, \@strings_xml_opts);
  make_items_xml($saver, \@xml_opts, \%pixkeys);
  make_settings_xml($saver, \@all_settings, \@settings_xml_opts);

  make_glue($saver, @glue_hacks);

  return 0;
}


sub make_manifest($$) {
  my ($saver, @manifest_opts) = @_;
  push @manifest_opts, $saver unless grep{$_ eq $saver} @manifest_opts;
  my $hack = ucfirst($saver);
  my $file = "project/xscreensaver/AndroidManifest.xml";
  open (my $in, '>', $file) || error ("$file: $!");

  my $body = ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" .
              "<manifest " .
              "xmlns:android=\"http://schemas.android.com/apk/res/android\" " .
              "package=\"org.jwz.xscreensaver\"\n" .
              "android:versionCode=\"1\"\n" .
              "android:versionName=\"1.0\">\n" .
              "<uses-sdk android:minSdkVersion=\"14\" " .
              "android:targetSdkVersion=\"19\" />\n" .
              "<application android:icon=\"\@drawable/thumbnail\" " .
              "android:label=\"\@string/app_name\" " .
              "android:name=\".XscreensaverApp\">\n\n");

  foreach my $save (@manifest_opts) {
      my $hac = ucfirst($save);
      $body = $body . ("<service android:label=\"\@string/" . $save . 
              "_name\" android:name=\".gen." . $hac . "Service\" " .
              "android:permission=\"android.permission.BIND_WALLPAPER\">\n" .
              " <intent-filter>\n" .
              "   <action " .
              "android:name=\"android.service.wallpaper.WallpaperService\" " .
              "/>\n" .
              " </intent-filter>\n" .
              " <meta-data android:name=\"android.service.wallpaper\" " .
              "android:resource=\"\@xml/" . $save . "\" />\n" .
              "</service>\n" .
              "<activity " .
              "android:label=\"\@string/" . $save . "_settings\" " .
              "android:name=\"org.jwz.xscreensaver.gen." . $hac . 
              "Settings\" " .
              "android:theme=\"\@android:style/Theme.Light.WallpaperSettings\" " .
              "android:exported=\"true\">\n" .
              "</activity>\n\n");

  }

  $body = $body . ("</application>\n\n" .
              "<uses-sdk android:minSdkVersion=\"14\" />\n" .
              "<uses-feature " .
              "android:name=\"android.software.live_wallpaper\" " .
              "android:required=\"true\" />\n" .
              "</manifest>\n");

  print $in $body;
  close $in;
}


sub make_hack_settings_xml($$) {

  my ($saver, @xml_opts) = @_;
  my $hack = ucfirst($saver);
  my $file = "project/xscreensaver/res/xml/" . $saver . "_settings.xml";
  my (%saver_keys) = get_keys_and_values($saver, @xml_opts);

  open (my $in, '>', $file) || error ("$file: $!");

  my $body = ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" .
              "<PreferenceScreen xmlns:android=" .
              "\"http://schemas.android.com/apk/res/android\">\n" .
              "    <PreferenceCategory\n" .
              "        android:title=\"\@string/" . $saver .
              "_settings\"\n" .
              "        android:key=\"" . $saver .
              "wallpaper_settings\">\n");

  my @keyarray = keys %saver_keys;


  foreach my $sgkey (@keyarray) {

    my $type = get_type($sgkey, @xml_opts);


    if ($type eq "number") {
        $body = $body . "    <org.jwz.xscreensaver.SliderPreference\n" .
              "        android:defaultValue=\"\@string/" . $saver .
              "_" . $sgkey .
              "_float\"\n" .
              "        android:dialogMessage=\"\@string/" . $saver .
              "_" . $sgkey .
              "_settings_summary\"\n" .
              "        android:key=\"" . $saver . "_" . $sgkey .
              "\"\n" .
              "        android:summary=\"\@array/" . $saver . "_" . $sgkey .
              "_prefix\"\n" .
              "        android:title=\"\@string/" . $saver . "_" . $sgkey .
              "_settings_title\" />\n";
     } else {
         $body = $body .  "    <ListPreference\n" .
              "            android:key=\"" . $saver . "_" . $sgkey .
              "\"\n" .
              "            android:title=\"\@string/" . $saver . "_" . $sgkey .
              "_settings_title\"\n" .
              "            android:summary=\"\@string/$saver" . "_" . $sgkey .
              "_settings_summary\"\n" .
              "            android:entries=\"\@array/$saver" . "_$sgkey" .
              "_names\"\n" .
              "            android:defaultValue=\"\@string/" . $saver . 
              "_" . $sgkey . "_default" . "\"\n" .
              "            android:entryValues=\"\@array/$saver" .
              "_$sgkey" .
              "_prefix\" />\n";
     }
  }

  $body = $body .   "    </PreferenceCategory>\n" .
              "</PreferenceScreen>\n";

  print $in $body;
  close $in;
}


sub make_items_xml($\@\%) {
  my $saver = $_[0];
  my @xml_opts = @{$_[1]};
  my %pixkeys = %{$_[2]};

  my $file = "project/xscreensaver/res/values/items.xml";

  my $body = ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" .
              "<resources>\n");

  while(my($key, $value) = each %pixkeys) {
      $body = $body . "    <item name=\"" . $key .
             "\" format=\"float\" type=\"string\">". $value . "</item>\n";

  }

  my (%saver_keys) = get_keys_and_values($saver, @xml_opts);
  my @keyarray = keys %saver_keys;

  foreach my $item_key (@keyarray) {

    my $type = get_type($item_key, @xml_opts);

    if ($type eq "number") {

      my ($low, $high, $default) = get_low_high_def($item_key, @xml_opts);
      my $float = ($default - $low) / ($high - $low);

      $body = ($body .
              "    <item name=\"" . $saver . "_" . $item_key .
              "_float\" format=\"float\" type=\"string\">$float</item>\n");
    }
  }

  $body =    ($body .
              "</resources>\n");
  open (my $in, '>', $file) || error ("$file: $!");
  print $in $body;
  close $in;
}


sub get_type($@) {

    my($type_key, @xml_opts) = @_;
    my $type='';

    foreach my $claim (@xml_opts) {

        my ($res, $compare, $xval) = ($claim =~ m/^(.*) (=|!=) (.*)$/s);
        if ($res eq $type_key . "_type") {
            $type = $xval;
        }

    }
    return $type;

}


sub get_low_high_def($@) {

    my($sgkey, @xml_opts) = @_;

    my $low;
    my $high;
    my $default;

    foreach my $claim (@xml_opts) {
        my ($res, $compare, $xval) = ($claim =~ m/^(.*) (=|!=) (.*)$/s);
        if ($res eq $sgkey . "_low") {
            $low = $xval;
        }
        elsif ($res eq $sgkey . "_high") {
            $high = $xval;
        }
        elsif ($res eq $sgkey) {
            $default = $xval;
        }
    }

    return ($low, $high, $default);

}


sub make_settings_xml($\@\@) {

  my $saver = $_[0];
  my @xml_opts = @{$_[1]};
  my @old_settings_xml = @{$_[2]};
  my $file = "project/xscreensaver/res/values/settings.xml";

  my $body = ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" .
              "<resources " .
              "xmlns:xliff=\"urn:oasis:names:tc:xliff:document:1.2\">\n");

  my $arrays_from_old_settings = old_settings_string_arrays(@old_settings_xml);

  $body = $body . $arrays_from_old_settings;

  my (%saver_keys) = get_keys_and_values($saver, @xml_opts);
  my @key_array = keys %saver_keys;

  my ($low, $high, $low_label, $high_label, $type);
  my @selects;

  # for each setting of the hack which we chose to add
  foreach my $selected_setting_key (@key_array) {
      # see what values were in the relevant xml in hacks/config for this hack
      foreach my $claim (@xml_opts) {
	    my ($xres, $xcompare, $xval) = ($claim =~ m/^(.*) (=|!=) (.*)$/s);
	    if ($xres =~ /^$selected_setting_key/) {
		my $one, my $two;
		if ($xres =~ /_/ ) {
		    ($one, $two) = ($xres =~ m/^(.*)_(.*)$/s);
                    if ($two eq "type") {
                        $type = $xval;
                    } elsif ($two eq "low-label") {
                        $low_label = $xval;
                    } elsif ($two eq "high-label") {
                        $high_label = $xval;
                    } elsif ($two eq "low") {
                        $low = $xval;
                    } elsif ($two eq "high") {
                        $high = $xval;
                    }
		} else {
		    $one = $xres;
                    if ($type eq "select") {
                        push @selects, $xval;
                    }
		}
            }
       }

       # add setting values based on the setting type (boolean, number, select)
       if ($type eq "boolean") {
           $body = $body . "    <string-array name=\"" . $saver .
           "_" . $selected_setting_key . "_names" . "\">\n" .
           "        <item>\"True\"</item>\n" .
           "        <item>\"False\"</item>\n" .
           "    </string-array>\n" .
           "    <string-array name=\"" . $saver . "_" . $selected_setting_key .
           "_prefix" . "\">\n" .
           "        <item>\@string/t</item>\n" .
           "        <item>\@string/f</item>\n" .
           "    </string-array>\n";
       } elsif ($type eq "number") {
           $body = $body . "    <string-array name=\"" . $saver .
           "_" . $selected_setting_key . "_names" . "\">\n" .
           "        <item>\"" . $low_label . "\"</item>\n" .
           "        <item>\"" . $high_label . "\"</item>\n" .
           "    </string-array>\n" .
           "    <string-array name=\"" . $saver . "_" . $selected_setting_key .
           "_prefix" . "\">\n" .
           "        <item>\"" . $low . "\"</item>\n" .
           "        <item>\"" . $high . "\"</item>\n" .
           "    </string-array>\n";
       } elsif ($type eq "select") {
           $body = $body . "    <string-array name=\"" . $saver .
           "_" . $selected_setting_key . "_names" . "\">\n";

           foreach my $item (@selects) {
               $body = $body . "        <item>\"" . $item . "\"</item>\n" ;
           }

           $body = $body . "    </string-array>\n" .
           "    <string-array name=\"" . $saver .
           "_" . $selected_setting_key . "_prefix" . "\">\n";

           foreach my $item (@selects) {
               $body = $body . "        <item>\"" . $item . "\"</item>\n" ;
           }

           $body = $body . "    </string-array>\n";
       }

       @selects=();
  }

  $body =    ($body .
              "</resources>\n");

  open (my $in, '>', $file) || error ("$file: $!");
  print $in $body;
  close $in;

}


sub old_settings_string_arrays(@) {

  my (@old_settings_file) = @_;

  my $body = '';
  my $current_string_array='';


  foreach my $claim (@old_settings_file) {
    my ($res, $compare, $xval) = ($claim =~ m/^(.*) (=) (.*)$/s);
    error ("unparsable xml claim: $_") unless $compare;

    if ($current_string_array ne $res) {
        if (length($current_string_array) > 0) {
           $body = $body . "    </string-array>\n";
        }

        $current_string_array = $res;
        $body = $body .  "    <string-array name=\"" . $current_string_array .
                         "\">\n";
    }

    $body = $body . "        <item>" . $xval . "</item>\n";
  }

  if ($#old_settings_file > -1) {
      $body = $body . "    </string-array>\n";
  }


  return $body;

}


# TODO: This adds the proper parameters to settings such as hilbert's, but it
# does not remove the improper parameters from hacks such as Hilbert yet.
#
sub get_settings($$\@) {
  my $saver = $_[0];
  my $switchmap = $_[1];
  my @xml_opts = @{$_[2]};

  my @keys = keys % { $switchmap};

  my $res_seen = 0;
  my $val_seen = 0;
  my @also;
  foreach my $sgkey (@keys) {
      my ($k, $v) = ($switchmap->{$sgkey} =~ m/^(.*): (.*)$/);

      if ($v ne '%') {
          foreach my $claim (@xml_opts) {
               my ($res, $compare, $val) = ($claim =~ m/^(.*) (=|!=) (.*)$/s);
               if ($res eq $k && $val eq $v) {
                   $val_seen = $val_seen + 1;
               }
               elsif ($res eq $k) {
                   $res_seen = $res_seen + 1;
               }
          }

          if ($val_seen eq 0 && $res_seen > 0) {
              my $so = "$k != $v";
              push @also, $so;
          }

          $val_seen = 0;
          $res_seen = 0;
      }
  }

  my @all = (@xml_opts, @also);
  return @all;

}


sub make_strings_xml($\@\@) {

  my $saver = $_[0];
  my @xml_opts = @{$_[1]};
  my @strings_xml_opts = @{$_[2]};

  my $saver_name = $saver . "_name";
  my $hack = ucfirst($saver);
  my $file = "project/xscreensaver/res/values/strings.xml";
  my (%saver_keys) = get_keys_and_values($saver, @xml_opts);

  my $body = ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" .
              "<resources>\n" .
              "    <string name=\"hello\">Hello World!</string>\n" .
              "    <string name=\"service_label\">Xscreensaver</string>\n" .
              "    <string name=\"description\">A live wallpaper</string>\n\n" .
              "    <string name=\"app_name\">Xscreensaver</string>\n" .
              "    <string name=\"author\">jwz and helpers</string>\n" .
              "    <string name=\"t\">True</string>\n" .
              "    <string name=\"f\">False</string>\n");

  foreach my $claim (@strings_xml_opts) {
    my ($res, $compare, $xval) = ($claim =~ m/^(.*) (=|!=) (.*)$/s);
    error ("$saver: unparsable xml claim: $_") unless $compare;
    if ($res eq 'hello' ||
        $res eq 'service_label' ||
        $res eq 'description' ||
        $res eq 'app_name' ||
        $res eq 'author' ||
        $res eq 't' ||
        $res eq 'f') {
    }
    elsif ($res eq $saver_name) {
        error ("$saver: $saver already in $file");
    }
    else {
        $body = ($body .
                 "    <string name=\"" . $res . "\">" . $xval . "</string>\n");
    }
  }

  $body =    ($body .
              "    <string name=\"" . $saver . "_name\">" . $hack .  
              "</string>\n" .
              "    <string name=\"" . $saver . 
              "_settings\">Settings</string>\n" .
              "    <string name=\"" . $saver . "_description\">" . $hack .  

              "</string>\n");

  my @keyarray = keys %saver_keys;

  foreach my $sgkey (@keyarray) {

    my $type = get_type($sgkey, @xml_opts);

    if ($type eq "number") {

	  my ($low, $high, $default) = get_low_high_def($sgkey, @xml_opts);
          my $float = ($default - $low) / ($high - $low);

          $body = ($body . "    <string name=\"" . $saver . "_" . $sgkey .
              "_settings_title\">" . "Set " . $sgkey . "</string>\n" .
              "    <string name=\"" . $saver . "_" . $sgkey .
              "_settings_summary\">" . "Choose " . $sgkey . "</string>\n" .
              "    <string name=\"" . $saver . "_" . $sgkey .
              "_low\">" . $low . "</string>\n" .
              "    <string name=\"" . $saver . "_" . $sgkey .
              "_high\">" . $high . "</string>\n" .
              "    <string name=\"" . $saver . "_" . $sgkey .
              "_default\">" . $saver_keys{$sgkey} . "</string>\n");
    }
      else {

              $body = ($body . "    <string name=\"" . $saver . "_" . $sgkey .
              "_settings_title\">" . "Set " . $sgkey . "</string>\n" .
              "    <string name=\"" . $saver . "_" . $sgkey .  

              "_settings_summary\">" . "Choose " . $sgkey . "</string>\n" .
              "    <string name=\"" . $saver . "_" . $sgkey .  
              "_default\">" . $saver_keys{$sgkey} . "</string>\n");
      }
  }

  $body =    ($body .
              "</resources>\n");

  open (my $in, '>', $file) || error ("$file: $!");
  print $in $body;
  close $in;
}


sub make_hack_xml($) {
  my ($saver) = @_;
  my $hack = ucfirst($saver);

  my $dir = "project/xscreensaver/res/xml/";
  my $file = $dir . $saver . ".xml";
  my $in;

  if (-d $dir) {
      open ($in, '>', $file) || error ("$file: $!");
  }
  else {
      mkdir $dir;
      open ($in, '>', $file) || error ("$file: $!");
  }

  my $body = ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" .
              "<wallpaper xmlns:android=" .
              "\"http://schemas.android.com/apk/res/android\"\n" .
              "   android:description=\"\@string/" . $saver .
              "_description\"\n" .
              "   android:settingsActivity=\"org.jwz.xscreensaver.gen.$hack" .
              "Settings\"\n" .
              "   android:thumbnail=\"\@drawable/" . $saver .
              "\" />\n");

  print $in $body;
  close $in;
}


sub make_glue($$) {
  my ($saver, @glue_hacks) = @_;
  my (@hacks) = @glue_hacks;

  push @hacks, $saver;

  my $dir = "gen/";
  my $file = $dir . "glue.c";
  my $in;

  if (-d $dir) {
      open ($in, '>', $file) || error ("$file: $!");
  }
  else {
      mkdir $dir;
      open ($in, '>', $file) || error ("$file: $!");
  }


  my $body = ("#include <jni.h>\n" .
              "#include <math.h>\n" .
              "#include <stdlib.h>\n" .
              "#include <stdio.h>\n" .
              "#include <time.h>\n" .
              "#include <pthread.h>\n" .
              "#include <GLES/gl.h>\n\n" .
              "#include \"screenhackI.h\"\n" .
              "#include \"jwzglesI.h\"\n" .
              "#include \"version.h\"\n\n" .
              "void drawXscreensaver();\n\n" .
              "int sWindowWidth = 0;\n" .
              "int sWindowHeight = 0;\n" .
              "int initTried = 0;\n" .
              "int renderTried = 0;\n" .
              "int resetTried = 0;\n" .
              "int currentFlip = 0;\n\n" .
              "pthread_mutex_t mutg = PTHREAD_MUTEX_INITIALIZER;\n\n" .
              "extern struct xscreensaver_function_table " .
              "*xscreensaver_function_table;\n\n" .
              "// if adding a table here, increase the magic number\n" .
              "struct xscreensaver_function_table\n");

              for my $i (0 .. $#hacks) {
                $body = $body . "    *" . $hacks[$i] ;
                $body = $body . "_xscreensaver_function_table";
                if ($i eq $#hacks  ) {
                  $body = $body . ";\n\n";
                }
                else {
                  $body = $body . ",\n";
                }
              }

  $body = $body . "struct running_hack {\n" .
              "    struct xscreensaver_function_table *xsft;\n" .
              "    Display *dpy;\n" .
              "    Window window;\n" .
              "    void *closure;\n" .
              "};\n\n" .
              "const char *progname;\n" .
              "const char *progclass;\n\n" .
              "struct running_hack rh[";
  $body = $body . scalar(@hacks);
  $body = $body . "];\n" .
              "// ^ magic number of hacks - TODO: remove magic number\n\n\n" .
              "int chosen;\n" .
              "JNIEXPORT void JNICALL\n" .
              "    Java_org_jwz_xscreensaver_CallNative_nativeInit\n" .
              "    (JNIEnv * env);\n" .
              "JNIEXPORT void JNICALL\n" .
              "    Java_org_jwz_xscreensaver_CallNative_nativeResize\n" .
              "    (JNIEnv * env, jobject thiz, jint w, jint h);\n" .
              "JNIEXPORT void JNICALL\n" .
              "    Java_org_jwz_xscreensaver_CallNative_nativeRender\n" .
              "    (JNIEnv * env);\n" .
              "JNIEXPORT void JNICALL\n" .
              "    Java_org_jwz_xscreensaver_CallNative_nativeDone\n" .
              "    (JNIEnv * env);\n";

  foreach my $bighack (@hacks) {
      my $bh = ucfirst($bighack);
      $body = $body . "JNIEXPORT void JNICALL\n" .
              "    Java_org_jwz_xscreensaver_gen_" . $bh . 
              "Wallpaper_allnativeSettings\n" .
              "    (JNIEnv * env, jobject thiz, jstring jhack," .
              " jstring hackPref,\n" .
              "     jint draw, jstring key);\n";

  }

  $body = $body . "\n\n\nvoid doinit()\n{\n\n" ;

  for my $j (0 .. $#hacks) {
    if ($j == 0) {
      $body = $body . "    if (chosen == " . $j . ") {\n" ;
    } elsif ($j == $#hacks) {
      $body = $body .         "    } else {\n" ;
    } else {
      $body = $body . "    } else if (chosen == " . $j . ") {\n";
    }
      $body = $body .  "	progname = \"" . $hacks[$j] . "\";\n" .
              "	rh[chosen].xsft = &" . $hacks[$j] . 
              "_xscreensaver_function_table;\n" ;
    }

  $body = $body . "    }\n\n" ;
  $body = $body . "    rh[chosen].dpy = jwxyz_make_display(0, 0);\n" .
              "    rh[chosen].window = XRootWindow(rh[chosen].dpy, 0);\n" .
              "// TODO: Zero looks right, " .
              "but double-check that is the right number\n\n" .
              "    progclass = rh[chosen].xsft->progclass;\n\n" .
              "    if (rh[chosen].xsft->setup_cb)\n" .
              "	rh[chosen].xsft->setup_cb(rh[chosen].xsft,\n" .
              "				  rh[chosen].xsft->setup_arg);\n\n" .
              "    if (resetTried < 1) {\n" .
              "	resetTried++;\n" .
              "        jwzgles_reset();\n" .
              "    }\n\n" .
              "    void *(*init_cb) (Display *, Window, void *) =\n" .
              "	(void *(*)(Display *, Window, void *)) " .
              "rh[chosen].xsft->init_cb;\n\n" .
              "    rh[chosen].closure =\n" .
              "	init_cb(rh[chosen].dpy, rh[chosen].window,\n" .
              "		rh[chosen].xsft->setup_arg);\n\n}\n\n\n" .
              "void drawXscreensaver()\n{\n" .
              "    pthread_mutex_lock(&mutg);\n" .
              "    rh[chosen].xsft->draw_cb(rh[chosen].dpy, " .
              "rh[chosen].window,\n" .
              "			     rh[chosen].closure);\n" .
              "    pthread_mutex_unlock(&mutg);\n\n}\n\n\n" .
              "JNIEXPORT void JNICALL\n" .
              "    Java_org_jwz_xscreensaver_CallNative_nativeInit\n" .
              "    (JNIEnv * env) {\n\n" .
              "    if (initTried < 1) {\n" .
              "	initTried++;\n" .
              "    } else {\n" .
              "	if (!rh[chosen].dpy) {\n" .
              "	    doinit();\n" .
              "	} else {\n" .
              "	    rh[chosen].xsft->free_cb(rh[chosen].dpy, " .
              "rh[chosen].window,\n" .
              "				     rh[chosen].closure);\n" .
              "	    jwxyz_free_display(rh[chosen].dpy);\n" .
              "	    rh[chosen].dpy = NULL;\n" .
              "	    rh[chosen].window = NULL;\n" .
              "	    if (!rh[chosen].dpy) {\n" .
              "		doinit();\n" .
              "	    }\n\n        }\n" .
              "    }\n\n}\n\n\n" .
              "JNIEXPORT void JNICALL\n" .
              "    Java_org_jwz_xscreensaver_CallNative_nativeResize\n" .
              "    (JNIEnv * env, jobject thiz, jint w, jint h) {\n\n" .
              "    sWindowWidth = w;\n" .
              "    sWindowHeight = h;\n\n" .
              "    if (!rh[chosen].dpy) {\n" .
              "	doinit();\n" .
              "    }\n\n" .
              "    jwxyz_window_resized(rh[chosen].dpy, " .
              "rh[chosen].window, 0, 0, w, h, 0);\n\n" .
              "    rh[chosen].xsft->reshape_cb(rh[chosen].dpy, " .
              "rh[chosen].window,\n" .
              "				rh[chosen].closure, w, h);\n}\n\n" .
              "JNIEXPORT void JNICALL\n" .
              "    Java_org_jwz_xscreensaver_CallNative_nativeRender\n" .
              "    (JNIEnv * env) {\n" .
              "    if (renderTried < 1) {\n" .
              "	renderTried++;\n" .
              "    } else {\n" .
              "	drawXscreensaver();\n" .
              "    }\n}\n\n" .
              "// TODO: Check Java side is calling this properly\n" .
              "JNIEXPORT void JNICALL\n" .
              "    Java_org_jwz_xscreensaver_CallNative_nativeDone\n" .
              "    (JNIEnv * env) {\n\n" .
              "    rh[chosen].xsft->free_cb(rh[chosen].dpy, " .
              "rh[chosen].window,\n" .
              "			     rh[chosen].closure);\n" .
              "    jwxyz_free_display(rh[chosen].dpy);\n" .
              "    rh[chosen].dpy = NULL;\n" .
              "    rh[chosen].window = NULL;\n\n}\n\n" ;

  for my $j (0 .. $#hacks) {
    my $jhack =  ucfirst($hacks[$j]);

    $body = $body . "JNIEXPORT void JNICALL\n" .
              "    Java_org_jwz_xscreensaver_gen_" . $jhack . 
              "Wallpaper_allnativeSettings\n" .
              "    (JNIEnv * env, jobject thiz, jstring jhack," .
              " jstring hackPref,\n" .
              "     jint draw, jstring key) {\n\n" .
              "    const char *chack = " .
              "(*env)->GetStringUTFChars(env, hackPref, NULL);\n" .
              "    char *hck = (char *) chack;\n" .
              "    const char *kchack = " .
              "(*env)->GetStringUTFChars(env, key, NULL);\n" .
              "    char *khck = (char *) kchack;\n\n" .
              "    if (draw == 2) {\n" .
              "        set" . $jhack . "Settings(hck, khck);\n" .
              "    }\n\n" .
              "    chosen = " . $j . ";\n}\n\n";

  }


  print $in $body;
  close $in;
}

sub make_wallpaper($$) {
  my ($saver, @xml_opts) = @_;
  my $hack = ucfirst($saver);
  my $file = "project/xscreensaver/src/org/jwz/xscreensaver/gen/";
  $file = $file . $hack . "Wallpaper.java";
  my (%saver_keys) = get_keys_and_values($saver, @xml_opts);

  open (my $in, '>', $file) || error ("$file: $!");

  my $body = ("package org.jwz.xscreensaver.gen;\n" .
              "import javax.microedition.khronos.egl.EGLConfig;\n" .
              "import javax.microedition.khronos.opengles.GL10;\n" .
              "import net.rbgrn.android.glwallpaperservice.*;\n" .
              "import android.opengl.GLU;\n" .
              "import android.content.Context;\n" .
              "import android.content.SharedPreferences;\n" .
              "import org.jwz.xscreensaver.*;\n" .
              "public class " . $hack .
              "Wallpaper extends ARenderer {\n" .
              "    private static native void allnativeSettings(" .
              "String hack, String hackPref, int draw, String key);\n" .
              "    public static final String SHARED_PREFS_NAME=\"" . $saver .
              "settings\";\n" .
              "    CallNative cn;\n" .
              "    public void onSurfaceCreated(" .
              "GL10 gl, EGLConfig config) {\n" .
              "        super.onSurfaceCreated(gl, config);\n" .
              "        cn = new CallNative();\n" .
              "        NonSurfaceCreated();\n" .
              "    }\n" .
              "    public void onDrawFrame(GL10 gl) {\n" .
              "        super.onDrawFrame(gl);\n" .
              "        allnativeSettings(\"bogus\", \"bogus\", 1, \"bogus\");\n" .
              "        NonDrawFrame();\n" .
              "    }\n" .
              "    void NonDrawFrame() {\n" .
              "        cn.nativeRender();\n" .
              "    }\n" .
              "    void doSP(SharedPreferences sspp) {\n" .


              "        String hack = \"" . $saver . "\";\n");

  my @keyarray = keys %saver_keys;
  foreach my $sgkey (@keyarray) {          

    my $type = get_type($sgkey, @xml_opts);

    if ($type eq "number") {

              my ($low, $high, $default) = get_low_high_def($sgkey, @xml_opts);
              my $float = ($default - $low) / ($high - $low);

              $body = $body .
              "        String " . $sgkey .
              "_low = sspp.getString(\"" . $saver .
              "_" . $sgkey . "_low\", \"". $low . "\");\n" .
              "        String " . $sgkey .
              "_high = sspp.getString(\"" . $saver .
              "_" . $sgkey . "_high\", \"" . $high . "\");\n" .
              "        Float " . $sgkey . "PrefF = sspp.getFloat(\"" . $saver .
              "_" . $sgkey . "\", " . $float . "f);\n" .
              "        String " . $sgkey . "Pref = getNumber(" . $sgkey .
              "_low, " . $sgkey . "_high, " . $sgkey . "PrefF);\n" .
              "        allnativeSettings(hack, " . $sgkey .
              "Pref, 2, \"" . $saver .  "_" . $sgkey . "\");\n";
    }
      elsif ($type eq "boolean") {

              $body = $body . "        String " . $sgkey .
              "Pref = sspp.getString(\"" . $saver .  "_" . $sgkey . 
              "\", \"" . $saver_keys{$sgkey} . "\");\n" .
              "        allnativeSettings(hack, " . $sgkey .
              "Pref, 2, \"" . $saver .  "_" . $sgkey . "\");\n";

      }
      elsif ($type eq "select") {

              $body = $body . "        String " . $sgkey .
              "Pref = sspp.getString(\"" . $saver .  "_" . $sgkey .
              "\", \"" . $saver_keys{$sgkey} . "\");\n" .
              "        allnativeSettings(hack, " . $sgkey .
              "Pref, 2, \"" . $saver .  "_" . $sgkey . "\");\n";

      }
      else {
          print STDERR "$progname: type $type not yet implemented \n";
      }

  }

  $body = $body . "    }\n" .
              "    String getNumber(String low, String high, Float pref) {\n" .
              "        Float lowF = Float.parseFloat(low);\n" .
              "        Float lowH = Float.parseFloat(high);\n" .
              "        Float diff = lowH - lowF;\n" .
              "        Float mult = pref * diff;\n" .
              "        Float add = mult + lowF;\n" .
              "        int i;\n" .
              "        String s;\n" .
              "        if (diff > 2.0) {\n" .
              "            i = (Integer) Math.round(add);\n" .
              "            s = Integer.toString(i);\n}\n" .
              "        else {\n" .
              "            s = Float.toString(add);\n}\n" .
              "        return s;\n" .
              "    }\n\n" .
              "    static\n" .
              "    {\n" .
              "        System.loadLibrary (\"xscreensaver\");\n" .
              "    }\n" .
              "}\n";

  print $in $body;
  close $in;

}

sub get_keys_and_values($$) {

  my ($saver, @xml_opts) = @_;
  my (%saver_keys) ;

  foreach my $claim (@xml_opts) {
    my ($res, $compare, $xval) = ($claim =~ m/^(.*) (=|!=) (.*)$/s);
    error ("$saver: unparsable xml claim: $_") unless $compare;

    if ($saver eq "sproingies") {
        if ($res eq "count") {
            $saver_keys{$res} = $xval;
        }
        elsif ($res eq "wireframe") {
            #$saver_keys{$res} = $xval;
            $saver_keys{$res} = "False";
        }

    }
    elsif ($saver eq "hilbert") {
        if ($res eq "mode") {
            $saver_keys{$res} = $xval;
        }
    }
    elsif ($saver eq "stonerview") {
        if ($res eq "transparent") {
            #$saver_keys{$res} = $xval;
            $saver_keys{$res} = "False";
        }
    }
    elsif ($saver eq "superquadrics") {
        # spinspeed/speed.  float/int
        if ($res eq "spinspeed") {
            $saver_keys{$res} = $xval;
        }
    }
    elsif ($saver eq "bouncingcow") {
        if ($res eq "count") {
            $saver_keys{$res} = "3";
        }
        elsif ($res eq "speed") {
            $saver_keys{$res} = "0.1";
        }
    }
    elsif ($saver eq "unknownpleasures") {
        if ($res eq "wireframe") {
            $saver_keys{$res} = "True";
        }
        elsif ($res eq "speed") {
            $saver_keys{$res} = "3.0";
        }
        #elsif ($res eq "count") {
        #    $saver_keys{$res} = $xval;
        #}
        #elsif ($res eq "resolution") {
        #    $saver_keys{$res} = $xval;
        #}
        #elsif ($res eq "ortho") {
        #    $saver_keys{$res} = $xval;
        #}

    }
    elsif ($saver eq "hypertorus") {
        if ($res =~ /^(displayMode|appearance|colors|projection3d|projection4d|speedwx|speedwy|speedwz|speedxy|speedxz|speedyz)$/) {
            $saver_keys{$res} = $xval;
        }
    }
    elsif ($saver eq "glhanoi") {
        if ($res =~ /^(light|fog|trails|poles|speed)$/) {
            # TODO: check in xval for true/false should be higher up in logic
            if ($xval =~ /^(true|false)$/) {
                $saver_keys{$res} = ucfirst($xval);
            }
            else {
                $saver_keys{$res} = $xval;
            }
        }
    }
    else {
        error ("$saver: not yet supported for Android");
    }

  }

  return (%saver_keys);
}


sub make_service($) {
  my ($saver) = @_;
  my $hack = ucfirst($saver);
  my $file = "project/xscreensaver/src/org/jwz/xscreensaver/gen/";
  $file = $file . $hack . "Service.java";
  open (my $in, '>', $file) || error ("$file: $!");

  my $body = ("package org.jwz.xscreensaver.gen;\n\n" .
              "import net.rbgrn.android.glwallpaperservice.*;\n" .
              "import android.content.SharedPreferences;\n" .
              "import org.jwz.xscreensaver.*;\n\n" .
              "// Original code provided by Robert Green\n" .
              "// http://www.rbgrn.net/content/354-glsurfaceview-adapted-3d-live-wallpapers\n" .
              "public class " . $hack .
              "Service extends GLWallpaperService {\n\n" .
              "    SharedPreferences sp;\n\n" .
              "    public " . $hack .
              "Service() {\n" .
              "        super();\n" .
              "    }\n\n" .
              "    \@Override\n" .
              "    public void onCreate() {\n" .
              "        sp = ((XscreensaverApp)getApplication())." .
              "getThePrefs($hack" . "Wallpaper.SHARED_PREFS_NAME);\n" .
              "    }\n\n" .
              "    public Engine onCreateEngine() {\n" .
              "        MyEngine engine = new MyEngine();\n" .
              "        return engine;\n" .
              "    }\n\n" .
              "    class MyEngine extends GLEngine {\n" .
              "        " . $hack .
              "Wallpaper renderer;\n" .
              "        public MyEngine() {\n" .
              "            super();\n" .
              "            // handle prefs, other initialization\n" .
              "            renderer = new " . $hack .
              "Wallpaper();\n" .
              "            setEGLConfigChooser(8, 8, 8, 8, 16, 0);\n" .
              "            setRenderer(renderer);\n" .
              "            setRenderMode(RENDERMODE_CONTINUOUSLY);\n" .
              "        }\n\n" .
              "        public void onDestroy() {\n" .
              "            super.onDestroy();\n" .
              "            if (renderer != null) {\n" .
              "                renderer.release(); " .
              "// assuming yours has this method - it should!\n" .
              "            }\n" .
              "            renderer = null;\n" .
              "        }\n\n" .
              "        \@Override\n" .
              "        public void onVisibilityChanged(boolean visible) {\n" .
              "            super.onVisibilityChanged(visible);\n" .
              "            if (visible) {\n" .
              "                renderer.doSP(sp);\n" .
              "            }\n" .
              "        }\n\n" .
              "    }\n" .
              "    static\n" .
              "    {\n" .
              "        System.loadLibrary (\"xscreensaver\");\n" .
              "    }\n\n\n" .
              "}\n");

  print $in $body;
  close $in;

}

sub make_settings($) {
  my ($saver) = @_;
  my $hack = ucfirst($saver);
  my $dir = "project/xscreensaver/src/org/jwz/xscreensaver/gen/";
  my $file = $dir . $hack . "Settings.java";
  my $in;

  if (-d $dir) {
      open ($in, '>', $file) || error ("$file: $!");
  }
  else {
      mkdir $dir;
      open ($in, '>', $file) || error ("$file: $!");
  }

  my $body = ("/*\n" .
              " * Copyright (C) 2009 Google Inc.\n" .
              " *\n" .
              " * Licensed under the Apache License, Version 2.0 " .
              "(the \"License\"); you may not\n" .
              " * use this file except in compliance with the License. " .
              "You may obtain a copy of\n" .
              " * the License at\n" .
              " *\n" .
              " * http://www.apache.org/licenses/LICENSE-2.0\n" .
              " *\n" .
              " * Unless required by applicable law or agreed to in writing," .
              " software\n" .
              " * distributed under the License is distributed" .
              " on an \"AS IS\" BASIS, WITHOUT\n" .
              " * WARRANTIES OR CONDITIONS OF ANY KIND," .
              " either express or implied. See the\n" .
              " * License for the specific language governing" .
              "permissions and limitations under\n" .
              " * the License.\n" .
              " */\n\n" .
              "package org.jwz.xscreensaver.gen;\n\n" .
              "import org.jwz.xscreensaver.R;\n\n" .
              "import android.content.SharedPreferences;\n" .
              "import android.os.Bundle;\n" .
              "import android.preference.PreferenceActivity;\n\n" .
              "public class " . $hack .
              "Settings extends PreferenceActivity\n" .
              "    implements " .
              "SharedPreferences.OnSharedPreferenceChangeListener {\n\n" .
              "    \@Override\n" .
              "    protected void onCreate(Bundle icicle) {\n" .
              "        super.onCreate(icicle);\n" .
              "        getPreferenceManager().setSharedPreferencesName(\n" .
              "            " . $hack .
              "Wallpaper.SHARED_PREFS_NAME);\n" .
              "        addPreferencesFromResource(R.xml." . $saver .
              "_settings);\n" .
              "        getPreferenceManager().getSharedPreferences()." .
              "registerOnSharedPreferenceChangeListener(\n" .
              "            this);\n" .
              "    }\n\n" .
              "    \@Override\n" .
              "    protected void onResume() {\n" .
              "        super.onResume();\n" .
              "    }\n\n" .
              "    \@Override\n" .
              "    protected void onDestroy() {\n" .
              "        getPreferenceManager().getSharedPreferences()." .
              "unregisterOnSharedPreferenceChangeListener(\n" .
              "            this);\n" .
              "        super.onDestroy();\n" .
              "    }\n\n" .
              "    public void onSharedPreferenceChanged(" .
              "SharedPreferences sharedPreferences,\n" .
              "                                          String key) {\n" .
              "    }\n" .
              "}\n");

  print $in $body;
  close $in;
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose] files ...\n";
  exit 1;
}

sub main() {
  my @files = ();
  while ($#ARGV >= 0) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/) { $verbose++; }
    elsif (m/^-v+$/) { $verbose += length($_)-1; }
    elsif (m/^-./) { usage; }
    else { push @files, $_; }
#    else { usage; }
  }

  usage unless ($#files >= 0);
  my $failures = 0;
  foreach (@files) { $failures += parse_then_make($_); }
  exit ($failures);
}

main();
