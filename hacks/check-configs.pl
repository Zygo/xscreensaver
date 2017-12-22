#!/usr/bin/perl -w
# Copyright © 2008-2017 Jamie Zawinski <jwz@jwz.org>
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
#
# It also converts the hacks/config/ XML files into the Android XML files.
#
# Created:  1-Aug-2008.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.25 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;
my $debug_p = 0;


my $text_default_opts = '';
foreach (qw(text-mode text-literal text-file text-url text-program)) {
  my $s = $_; $s =~ s/-(.)/\U$1/g; $s =~ s/url/URL/si;
  $text_default_opts .= "{\"-$_\", \".$s\", XrmoptionSepArg, 0},\n";
}
my $image_default_opts = '';
foreach (qw(choose-random-images grab-desktop-images)) {
  my $s = $_; $s =~ s/-(.)/\U$1/g;
  $image_default_opts .= "{\"-$_\", \".$s\", XrmoptionSepArg, 0},\n";
}
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



# Returns two tables:
# - A table of the default resource values.
# - A table of "-switch" => "resource: value", or "-switch" => "resource: %"
#
sub parse_src($) {
  my ($saver) = @_;
  my $file = lc($saver) . ".c";

  # kludge...
  $file = 'apple2-main.c' if ($file eq 'apple2.c');
  $file = 'sproingiewrap.c' if ($file eq 'sproingies.c');
  $file = 'b_lockglue.c' if ($file eq 'bubble3d.c');
  $file = 'polyhedra-gl.c' if ($file eq 'polyhedra.c');
  $file = 'companion.c' if ($file eq 'companioncube.c');
  $file = 'rd-bomb.c' if ($file eq 'rdbomb.c');

  my $ofile = $file;
  $file = "glx/$ofile"          unless (-f $file);
  $file = "../hacks/$ofile"     unless (-f $file);
  $file = "../hacks/glx/$ofile" unless (-f $file);
  my $body = '';
  open (my $in, '<', $file) || error ("$ofile: $!");
  while (<$in>) { $body .= $_; }
  close $in;
  $file =~ s@^.*/@@;

  my $xlockmore_p = 0;
  my $thread_p = ($body =~ m/THREAD_DEFAULTS/);
  my $analogtv_p = ($body =~ m/ANALOGTV_DEFAULTS/);
  my $text_p = ($body =~ m/"textclient\.h"/);
  my $grab_p = ($body =~ m/load_image_async/);

  $body =~ s@/\*.*?\*/@@gs;
  $body =~ s@^#\s*(if|ifdef|ifndef|elif|else|endif).*$@@gm;
  $body =~ s/(THREAD|ANALOGTV)_(DEFAULTS|OPTIONS)(_XLOCK)?//gs;
  $body =~ s/__extension__//gs;

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
      $val =~ s/"\s*"\s*$//s;
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
  $res_to_val{textMode} = 'date';
  $res_to_val{textLiteral} = '';
  $res_to_val{textURL} =
    'https://en.wikipedia.org/w/index.php?title=Special:NewPages&feed=rss';
  $res_to_val{grabDesktopImages} = 'true';
  $res_to_val{chooseRandomImages} = 'true';

  print STDERR "$progname: $file:   progclass = $2\n" if ($verbose > 2);

  print STDERR "$progname: $file: switches to resources:\n"
    if ($verbose > 2);
  my %switch_to_res;
  $switch_to_res{'-fps'} = 'doFPS: true';
  $switch_to_res{'-fg'}  = 'foreground: %';
  $switch_to_res{'-bg'}  = 'background: %';
  $switch_to_res{'-no-grab-desktop-images'}  = 'grabDesktopImages: false';
  $switch_to_res{'-no-choose-random-images'}  = 'chooseRandomImages: false';

  my ($ign, $opts) = ($body =~ m/(_options|\bopts)\s*\[\]\s*=\s*{(.*?)}\s*;/s);
  if  ($xlockmore_p || $thread_p || $analogtv_p || $opts) {
    $opts = '' unless $opts;
    $opts .= ",\n$text_default_opts" if ($text_p);
    $opts .= ",\n$image_default_opts" if ($grab_p);
    $opts .= ",\n$xlockmore_default_opts" if ($xlockmore_p);
    $opts .= ",\n$thread_default_opts" if ($thread_p);
    $opts .= ",\n$analogtv_default_opts" if ($analogtv_p);

    foreach (split (/,\s*\n/, $opts)) {
      s/^\s*//s;
      s/\s*$//s;
      next if m/^$/s;
      next if m/^\{\s*0\s*,/s;
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

my %video_dups;

# Returns a list of:
#    "resource = default value"
# or "resource != non-default value"
#
# Also a hash of the simplified XML contents.
#
sub parse_xml($$$) {
  my ($saver, $switch_to_res, $src_opts) = @_;

  my $saver_title = undef;
  my $gl_p = 0;
  my $file = "config/" . lc($saver) . ".xml";
  my $ofile = $file;
  $file = "../hacks/$ofile" unless (-f $file);
  my $body = '';
  open (my $in, '<', $file) || error ("$ofile: $!");
  while (<$in>) { $body .= $_; }
  close $in;
  $file =~ s@^.*/@@;

  my @result = ();

  $body =~ s@<xscreensaver-text\s*/?>@
    <select id="textMode">
      <option id="date"  _label="Display the date and time"/>
      <option id="text"  _label="Display static text"
        arg-set="-text-mode literal"/>
      <option id="url"   _label="Display the contents of a URL"
        arg-set="-text-mode url"/>
    </select>
    <string id="textLiteral" _label="Text to display" arg="-text-literal %"/>
    <string id="textURL" _label="URL to display" arg="-text-url %"/>
    @gs;

  $body =~ s@<xscreensaver-image\s*/?>@
    <boolean id="grabDesktopImages" _label="Grab screenshots"
       arg-unset="-no-grab-desktop-images"/>
    <boolean id="chooseRandomImages" _label="Use photo library"
       arg-unset="-no-choose-random-images"/>
    @gs;

  $body =~ s/<!--.*?-->/ /gsi;

  $body =~ s@(<(_description)>.*?</\2>)@{ $_ = $1; s/\n/\002/gs; $_; }@gsexi;

  $body =~ s/\s+/ /gs;
  $body =~ s/</\001</gs;
  $body =~ s/\001(<option)/$1/gs;

  my $video = undef;

  my @widgets = ();

  print STDERR "$progname: $file: options:\n" if ($verbose > 2);
  foreach (split (m/\001/, $body)) {
    next if (m/^\s*$/s);
    my ($type, $args) = m@^<([?/]?[-_a-z]+)\b\s*(.*)$@si;
    error ("$progname: $file: unparsable: $_") unless $type;
    next if ($type =~ m@^/@);

    my $ctrl = { type => $type };

    if ($type =~ m/^( [hv]group |
                      \?xml |
                      command |
                      file |
                      xscreensaver-image |
                      xscreensaver-updater
                    )/sx) {
      $ctrl = undef;

    } elsif ($type eq '_description') {
      $args =~ s/\002/\n/gs;
      $args =~ s@^>\s*@@s;
      $args =~ s/^\n*|\s*$//gs;
      $ctrl->{text} = $args;

    } elsif ($type eq 'screensaver') {
      ($saver_title) = ($args =~ m/\b_label\s*=\s*\"([^\"]+)\"/s);
      ($gl_p) = ($args =~ m/\bgl="?yes/s);
      my $s = $saver_title;
      $s =~ s/\s+//gs;
      my $val = "progclass = $s";
      push @result, $val;
      print STDERR "$progname: $file:   name:    $saver_title\n"
        if ($verbose > 2);
      $ctrl = undef;

    } elsif ($type eq 'video') {
      error ("$file: multiple videos") if $video;
      ($video) = ($args =~ m/\bhref="(.*?)"/);
      error ("$file: unparsable video") unless $video;
      error ("$file: unparsable video URL")
        unless ($video =~ m@^https?://www\.youtube\.com/watch\?v=[^?&]+$@s);
      $ctrl = undef;

    } elsif ($type eq 'select') {
      $args =~ s/</\001</gs;
      my @opts = split (/\001/, $args);
      shift @opts;
      my $unset_p = 0;
      my $this_res = undef;
      my @menu = ();
      foreach (@opts) {
        error ("$file: unparsable option: $_") unless (m/^<option\s/);

        my %item;
        my $opt = $_;
        $opt =~ s@^<option\s+@@s;
        $opt =~ s@[?/]>\s*$@@s;
        while ($opt =~ s/^\s*([^\s]+)\s*=\s*"(.*?)"\s*(.*)/$3/s) {
          my ($k, $v) = ($1, $2);
          $item{$k} = $v;
        }

        error ("unparsable XML option line: $_ [$opt]") if ($opt);
        push @menu, \%item;

        my ($set) = $item{'arg-set'};
        if ($set) {
          my ($set2, $val) = ($set =~ m/^(.*?) (.*)$/s);
          $set = $set2 if ($set2);
          my ($res) = $switch_to_res->{$set};
          error ("$file: no resource for select switch \"$set\"") unless $res;

          my ($res2, $val2) = ($res =~ m/^(.*?): (.*)$/s);
          error ("$file: unparsable select resource: $res") unless $res2;
          $res = $res2;
          $val = $val2 unless ($val2 eq '%');
          $item{value} = $val;

          error ("$file: mismatched resources: $res vs $this_res")
            if (defined($this_res) && $this_res ne $res);
          $this_res = $res;

          $val = "$res != $val";
          push @result, $val;
          print STDERR "$progname: $file:   select:  $val\n" if ($verbose > 2);

        } else {
          error ("$file: multiple default options: $set") if ($unset_p);
          $unset_p++;
        }
      }
      $ctrl->{resource} = $this_res;
      $ctrl->{default} = $src_opts->{$this_res};
      $ctrl->{menu} = \@menu;

    } else {

      my $rest = $args;
      $rest =~ s@[/?]*>\s*$@@s;
      while ($rest =~ s/^\s*([^\s]+)\s*=\s*"(.*?)"\s*(.*)/$3/s) {
        my ($k, $v) = ($1, $2);
        $ctrl->{$k} = $v;
      }
      error ("unparsable XML line: $args [$rest]") if ($rest);

      if ($type eq 'number') {
        my ($arg) = $ctrl->{arg};
        my ($val) = $ctrl->{default};
        $val = '' unless defined($val);

        my $switch = $arg;
        $switch =~ s/\s+.*$//;
        my ($res) = $switch_to_res->{$switch};
        error ("$file: no resource for $type switch \"$arg\"") unless $res;

        $res =~ s/: \%$//;
        error ("$file: unparsable value: $res") if ($res =~ m/:/);
        $ctrl->{resource} = $res;

        $val = "$res = $val";
        push @result, $val;
        print STDERR "$progname: $file:   number:  $val\n" if ($verbose > 2);

      } elsif ($type eq 'boolean') {
        my ($set)   = $ctrl->{'arg-set'};
        my ($unset) = $ctrl->{'arg-unset'};
        my ($arg) = $set || $unset || error ("$file: unparsable: $args");
        my ($res) = $switch_to_res->{$arg};
          error ("$file: no resource for boolean switch \"$arg\"") unless $res;

        my ($res2, $val) = ($res =~ m/^(.*?): (.*)$/s);
        error ("$file: unparsable boolean resource: $res") unless $res2;
        $res = $res2;

        $ctrl->{resource} = $res;
        $ctrl->{convert} = 'invert' if ($val =~ m/off|false|no/i);
        $ctrl->{default} = ($ctrl->{convert} ? 'true' : 'false');

#       $val = ($set ? "$res != $val" : "$res = $val");
        $val = "$res != $val";
        push @result, $val;
        print STDERR "$progname: $file:   boolean: $val\n" if ($verbose > 2);

      } elsif ($type eq 'string') {
        my ($arg) = $ctrl->{arg};

        my $switch = $arg;
        $switch =~ s/\s+.*$//;
        my ($res) = $switch_to_res->{$switch};
        error ("$file: no resource for $type switch \"$arg\"") unless $res;

        $res =~ s/: \%$//;
        error ("$file: unparsable value: $res") if ($res =~ m/:/);
        $ctrl->{resource} = $res;
        $ctrl->{default} = $src_opts->{$res};
        my $val = "$res = %";
        push @result, $val;
        print STDERR "$progname: $file:   string:  $val\n" if ($verbose > 2);

      } else {
        error ("$file: unknown type \"$type\" for no arg");
      }
    }

    push @widgets, $ctrl if $ctrl;
  }

#  error ("$file: no video") unless $video;
  print STDERR "\n$file: WARNING: no video\n\n" unless $video;

  if ($video && $video_dups{$video} && 
      $video_dups{$video} ne $saver_title) {
    print STDERR "\n$file: WARNING: $saver_title: dup video with " .
      $video_dups{$video} . "\n";
  }
  $video_dups{$video} = $saver_title if ($video);

  return ($saver_title, $gl_p, \@result, \@widgets);
}


sub check_config($) {
  my ($saver) = @_;

  # kludge
  return 0 if ($saver =~ m/(-helper)$/);

  my ($src_opts, $switchmap) = parse_src ($saver);
  my ($saver_title, $gl_p, $xml_opts, $widgets) =
    parse_xml ($saver, $switchmap, $src_opts);

  my $failures = 0;
  foreach my $claim (@$xml_opts) {
    my ($res, $compare, $xval) = ($claim =~ m/^(.*) (=|!=) (.*)$/s);
    error ("$saver: unparsable xml claim: $claim") unless $compare;

    my $sval = $src_opts->{$res};
    if ($res =~ m/^TV|^text-mode/) {
      print STDERR "$progname: $saver: OK: skipping \"$res\"\n"
        if ($verbose > 1);
    } elsif (!defined($sval)) {
      print STDERR "$progname: $saver: $res: not in source\n";
    } elsif ($claim !~ m/ = %$/s &&
             ($compare eq '!='
              ? $sval eq $xval
              : $sval ne $xval)) {
      print STDERR "$progname: $saver: " .
        "src has \"$res = $sval\", xml has \"$claim\"\n";
      $failures++;
    } elsif ($verbose > 1) {
      print STDERR "$progname: $saver: OK: \"$res = $sval\" vs \"$claim\"\n";
    }
  }

  # Now make sure the progclass in the source and XML also matches
  # the XCode target name.
  #
  my $obd = "../OSX/build/Debug";
  if (-d $obd) {
    my $progclass = $src_opts->{progclass};
    $progclass = 'DNAlogo' if ($progclass eq 'DNALogo');
    my $f = (glob("$obd/$progclass.saver*"))[0];
    if (!$f && $progclass ne 'Flurry') {
      print STDERR "$progname: $progclass.saver does not exist\n";
      $failures++;
    }
  }

  print STDERR "$progname: $saver: OK\n"
    if ($verbose == 1 && $failures == 0);

  return $failures;
}


# Returns true if the two files differ (by running "cmp")
#
sub cmp_files($$) {
  my ($file1, $file2) = @_;

  my @cmd = ("cmp", "-s", "$file1", "$file2");
  print STDERR "$progname: executing \"" . join(" ", @cmd) . "\"\n"
    if ($verbose > 3);

  system (@cmd);
  my $exit_value  = $? >> 8;
  my $signal_num  = $? & 127;
  my $dumped_core = $? & 128;

  error ("$cmd[0]: core dumped!") if ($dumped_core);
  error ("$cmd[0]: signal $signal_num!") if ($signal_num);
  return $exit_value;
}


sub diff_files($$) {
  my ($file1, $file2) = @_;

  my @cmd = ("diff", 
             "-U1",
#            "-w",
             "--unidirectional-new-file", "$file1", "$file2");
  print STDERR "$progname: executing \"" . join(" ", @cmd) . "\"\n"
    if ($verbose > 3);

  system (@cmd);
  my $exit_value  = $? >> 8;
  my $signal_num  = $? & 127;
  my $dumped_core = $? & 128;

  error ("$cmd[0]: core dumped!") if ($dumped_core);
  error ("$cmd[0]: signal $signal_num!") if ($signal_num);
  return $exit_value;
}


# If the two files differ:
#   mv file2 file1
# else
#   rm file2
#
sub rename_or_delete($$;$) {
  my ($file, $file_tmp, $suffix_msg) = @_;

  my $changed_p = cmp_files ($file, $file_tmp);

  if ($changed_p && $debug_p) {
    print STDOUT "\n" . ('#' x 79) . "\n";
    diff_files ("$file", "$file_tmp");
    $changed_p = 0;
  }

  if ($changed_p) {

    if (!rename ("$file_tmp", "$file")) {
      unlink "$file_tmp";
      error ("mv $file_tmp $file: $!");
    }
    print STDERR "$progname: wrote $file" .
      ($suffix_msg ? " $suffix_msg" : "") . "\n";

  } else {
    unlink "$file_tmp" || error ("rm $file_tmp: $!\n");
    print STDERR "$file unchanged" .
                 ($suffix_msg ? " $suffix_msg" : "") . "\n"
        if ($verbose);
    print STDERR "$progname: rm $file_tmp\n" if ($verbose > 2);
  }
}


# Write the given body to the file, but don't alter the file's
# date if the new content is the same as the existing content.
#
sub write_file_if_changed($$;$) {
  my ($outfile, $body, $suffix_msg) = @_;

  my $file_tmp = "$outfile.tmp";
  open (my $out, '>', $file_tmp) || error ("$file_tmp: $!");
  (print $out $body) || error ("$file_tmp: $!");
  close $out || error ("$file_tmp: $!");
  rename_or_delete ($outfile, $file_tmp, $suffix_msg);
}


# Read the template file and splice in the @KEYWORDS@ in the hash.
#
sub read_template($$) {
  my ($file, $subs) = @_;
  my $body = '';
  open (my $in, '<', $file) || error ("$file: $!");
  while (<$in>) { $body .= $_; }
  close $in;

  $body =~ s@/\*.*?\*/@@gs;  # omit comments
  $body =~ s@//.*$@@gm;

  foreach my $key (keys %$subs) {
    my $val = $subs->{$key};
    $body =~ s/@\Q$key\E@/$val/gs;
  }

  if ($body =~ m/(@[-_A-Z\d]+@)/s) {
    error ("$file: unmatched: $1 [$body]");
  }

  $body =~ s/[ \t]+$//gm;
  $body =~ s/(\n\n)\n+/$1/gs;
  return $body;
}


# This is duplicated in OSX/update-info-plist.pl
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
               #"From the XScreenSaver collection:\n" .
               #"https://www.jwz.org/xscreensaver/\n" .
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


sub build_android(@) {
  my (@savers) = @_;

  my $package     = "org.jwz.xscreensaver";
  my $project_dir = "xscreensaver";
  my $xml_dir     = "$project_dir/res/xml";
  my $values_dir  = "$project_dir/res/values";
  my $java_dir    = "$project_dir/src/org/jwz/xscreensaver/gen";
  my $gen_dir     = "gen";

  my $xml_header = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";

  my $manifest = '';
  my $daydream_java = '';
  my $settings_java = '';
  my $wallpaper_java = '';
  my $fntable_h2 = '';
  my $fntable_h3 = '';
  my $arrays   = '';
  my $strings  = '';
  my %write_files;
  my %string_dups;

  my $vers;
  {
    my $file = "../utils/version.h";
    my $body = '';
    open (my $in, '<', $file) || error ("$file: $!");
    while (<$in>) { $body .= $_; }
    close $in;
    ($vers) = ($body =~ m@ (\d+\.\d+) @s);
    error ("$file: no version number") unless $vers;
  }


  foreach my $saver (@savers) {
    next if ($saver =~ m/(-helper)$/);
    $saver = 'rdbomb' if ($saver eq 'rd-bomb');

    my ($src_opts, $switchmap) = parse_src ($saver);
    my ($saver_title, $gl_p, $xml_opts, $widgets) =
      parse_xml ($saver, $switchmap, $src_opts);

    my $saver_class = "${saver_title}";
    $saver_class =~ s/\s+//gs;
    $saver_class =~ s/^([a-z])/\U$1/gs;  # upcase first letter

    $saver_title =~ s/(.[a-z])([A-Z\d])/$1 $2/gs;	# Spaces in InterCaps
    $saver_title =~ s/^(GL|RD)[- ]?(.)/$1 \U$2/gs;	# Space after "GL"
    $saver_title =~ s/^Apple ?2$/Apple &#x5D;&#x5B;/gs;	# "Apple ]["
    $saver_title =~ s/(m)oe(bius)/$1&#xF6;$2/gsi;	# &ouml;
    $saver_title =~ s/(moir)e/$1&#xE9;/gsi;		# &eacute;
    $saver_title =~ s/^([a-z])/\U$1/s;			# "M6502" for sorting

    my $settings = '';

    my $localize0 = sub($$) {
      my ($key, $string) = @_;
      $string =~ s@([\\\"\'])@\\$1@gs;		# backslashify
      $string =~ s@\n@\\n@gs;			# quote newlines
      $key =~ s@[^a-z\d_]+@_@gsi;		# illegal characters

      my $old = $string_dups{$key};
      error ("dup string: $key: \"$old\" != \"$string\"")
        if (defined($old) && $old ne $string);
      $string_dups{$key} = $string;

      my $fmt = ($string =~ m/%/ ? ' formatted="false"' : '');
      $strings .= "<string name=\"${key}\"$fmt>$string</string>\n"
        unless defined($old);
      return "\@string/$key";
    };

    $localize0->('app_name', 'XScreenSaver');

    $settings .= ("<Preference\n" .
                  "  android:key=\"${saver}_reset\"\n" .
                  "  android:title=\"" .
                      $localize0->('reset_to_defaults', 'Reset to defaults') .
                     "\"\n" .
                  " />\n");

    my $daydream_desc = '';
    foreach my $widget (@$widgets) {
      my $type  = $widget->{type};
      my $rsrc  = $widget->{resource};
      my $label = $widget->{_label};
      my $def   = $widget->{default};
      my $invert_p = (($widget->{convert} || '') eq 'invert');

      my $key   = "${saver}_$rsrc" if $rsrc;

      #### The menus don't actually have titles on X11 or Cocoa...
      $label = $widget->{resource} unless $label;

      my $localize = sub($;$) {
        my ($string, $suf) = @_;
        $suf = 'title' unless $suf;
        return $localize0->("${saver}_${rsrc}_${suf}", $string);
      };

      if ($type eq 'slider' || $type eq 'spinbutton') {

        my $low        = $widget->{low};
        my $high       = $widget->{high};
        my $float_p    = $low =~ m/[.]/;
        my $low_label  = $widget->{'_low-label'};
        my $high_label = $widget->{'_high-label'};

        $low_label  = $low  unless defined($low_label);
        $high_label = $high unless defined($high_label);

        ($low, $high) = ($high, $low)
          if (($widget->{convert} || '') eq 'invert');

        $settings .=
          ("<$package.SliderPreference\n" .
           "  android:layout=\"\@layout/slider_preference\"\n" .
           "  android:key=\"${key}\"\n" .
           "  android:title=\"" . $localize->($label) . "\"\n" .
           "  android:defaultValue=\"$def\"\n" .
           "  low=\"$low\"\n" .
           "  high=\"$high\"\n" .
           "  lowLabel=\""  . $localize->($low_label,  'low_label')  . "\"\n" .
           "  highLabel=\"" . $localize->($high_label, 'high_label') . "\"\n" .
           "  integral=\"" .($float_p ? 'false' : 'true'). "\" />\n");

      } elsif ($type eq 'boolean') {

        my $def = ($invert_p ? 'true' : 'false');
        $settings .=
          ("<CheckBoxPreference\n" .
           "  android:key=\"${key}\"\n" .
           "  android:title=\"" . $localize->($label) . "\"\n" .
           "  android:defaultValue=\"$def\" />\n");

      } elsif ($type eq 'select') {

        $label =~ s/^(.)/\U$1/s;  # upcase first letter of menu title
        $label =~ s/[-_]/ /gs;
        $label =~ s/([a-z])([A-Z])/$1 $2/gs;
        $def = '' unless defined ($def);
        $settings .=
          ("<ListPreference\n" .
           "  android:key=\"${key}\"\n" .
           "  android:title=\"" . $localize->($label, 'menu') . "\"\n" .
           "  android:entries=\"\@array/${key}_entries\"\n" .
           "  android:defaultValue=\"$def\"\n" .
           "  android:entryValues=\"\@array/${key}_values\" />\n");

        my $a1 = '';
        foreach my $item (@{$widget->{menu}}) {
          my $val = $item->{value};
          if (! defined($val)) {
            $val = $src_opts->{$widget->{resource}};
            error ("$saver: no default resource in option menu " .
                   $item->{_label})
              unless defined($val);
          }
          $val =~ s@([\\\"\'])@\\$1@gs;		# backslashify
          $a1 .= "  <item>$val</item>\n";
        }

        my $a2 = '';
        foreach my $item (@{$widget->{menu}}) {
          my $val = $item->{value};
          $val = $src_opts->{$widget->{resource}} unless defined($val);
          $a2 .= ("  <item>" . $localize->($item->{_label}, $val) .
                  "</item>\n");
        }

        my $fmt1 = ($a1 =~ m/%/ ? ' formatted="false"' : '');
        my $fmt2 = ($a2 =~ m/%/ ? ' formatted="false"' : '');
        $arrays .= ("<string-array name=\"${key}_values\"$fmt1>\n" .
                    $a1 .
                    "</string-array>\n" .
                    "<string-array name=\"${key}_entries\"$fmt2>\n" .
                    $a2 .
                    "</string-array>\n");

      } elsif ($type eq 'string') {

        $def =~ s/&/&amp;/gs;
        $settings .=
          ("<EditTextPreference\n" .
           "  android:key=\"${key}\"\n" .
           "  android:title=\"" . $localize->($label) . "\"\n" .
           "  android:defaultValue=\"$def\" />\n");

      } elsif ($type eq 'file') {

      } elsif ($type eq '_description') {

        $type = 'description';
        $rsrc = $type;
        my $desc = $widget->{text};
        (undef, $desc) = munge_blurb ($saver, $saver_title, $vers, $desc);

        # Lose the Wikipedia URLs.
        $desc =~ s@https?:.*?\b(wikipedia|mathworld)\b[^\s]+[ \t]*\n?@@gm;
        $desc =~ s/(\n\n)\n+/$1/s;
        $desc =~ s/\s*$/\n\n\n/s;

        $daydream_desc = $desc;

        my ($year) = ($daydream_desc =~ m/\b((19|20)\d\d)\b/s);
        error ("$saver: no year") unless $year;
        $daydream_desc =~ s/^.*?\n\n//gs;
        $daydream_desc =~ s/\n.*$//gs;
        $daydream_desc = "$year: $daydream_desc";
        $daydream_desc =~ s/^(.{72}).+$/$1.../s;

        $settings .=
          ("<Preference\n" .
           "  android:icon=\"\@drawable/thumbnail\"\n" .
           "  android:key=\"${saver}_${type}\"\n" .
#           "  android:selectable=\"false\"\n" .
           "  android:persistent=\"false\"\n" .
           "  android:layout=\"\@layout/preference_blurb\"\n" .
           "  android:summary=\"" . $localize->($desc) . "\">\n" .
           "  <intent android:action=\"android.intent.action.VIEW\"\n" .
           "    android:data=\"https://www.jwz.org/xscreensaver/\" />\n" .
           "</Preference>\n");

      } else {
        error ("unhandled type: $type");
      }
    }

    my $heading = "XScreenSaver: $saver_title";

    $settings =~ s/^/  /gm;
    $settings = ($xml_header .
                 "<PreferenceScreen xmlns:android=\"" .
                 "http://schemas.android.com/apk/res/android\"\n" .
                 "  android:title=\"" .
                 $localize0->("${saver}_settings_title", $heading) . "\">\n" .
                 $settings .
                 "</PreferenceScreen>\n");

    my $saver_underscore = $saver;
    $saver_underscore =~ s/-/_/g;
    $write_files{"$xml_dir/${saver_underscore}_settings.xml"} = $settings;

    $manifest .= ("<service android:label=\"" .
                     $localize0->("${saver_underscore}_saver_title",
                                  $saver_title) .
                     "\"\n" .
                  "  android:summary=\"" .
                       $localize0->("${saver_underscore}_saver_desc",
                                    $daydream_desc) . "\"\n" .
                  "  android:name=\".gen.Daydream\$$saver_class\"\n" .
                  "  android:permission=\"android.permission" .
                       ".BIND_DREAM_SERVICE\"\n" .
                  "  android:exported=\"true\"\n" .
                  "  android:icon=\"\@drawable/${saver_underscore}\">\n" .
                  "  <intent-filter>\n" .
                  "    <action android:name=\"android.service.dreams" .
                        ".DreamService\" />\n" .
                  "    <category android:name=\"android.intent.category" .
                        ".DEFAULT\" />\n" .
                  "  </intent-filter>\n" .
                  "  <meta-data android:name=\"android.service.dream\"\n" .
                  "    android:resource=\"\@xml/${saver}_dream\" />\n" .
                  "</service>\n" .
                  "<service android:label=\"" .
                     $localize0->("${saver_underscore}_saver_title",
                                  $saver_title) .
                     "\"\n" .
                  "  android:summary=\"" .
                       $localize0->("${saver_underscore}_saver_desc",
                                    $daydream_desc) . "\"\n" .
                  "  android:name=\".gen.Wallpaper\$$saver_class\"\n" .
                  "  android:permission=\"android.permission" .
                       ".BIND_WALLPAPER\">\n" .
                  "  <intent-filter>\n" .
                  "    <action android:name=\"android.service.wallpaper" .
                        ".WallpaperService\" />\n" .
                  "    <category android:name=\"android.intent.category" .
                        ".DEFAULT\" />\n" . # TODO: Is the DEFAULT category needed?
                  "  </intent-filter>\n" .
                  "  <meta-data android:name=\"android.service.wallpaper\"\n" .
                  "    android:resource=\"\@xml/${saver}_wallpaper\" />\n" .
                  "</service>\n" .
                  "<activity android:label=\"" .
                   $localize0->("${saver}_settings_title", $heading) . "\"\n" .
                  "  android:name=\"$package.gen.Settings\$$saver_class\"\n" .
                  "  android:exported=\"true\">\n" .
                  "</activity>\n"
                 );

    my $dream = ("<dream xmlns:android=\"" .
                   "http://schemas.android.com/apk/res/android\"\n" .
                 "  android:settingsActivity=\"" .
                     "$package.gen.Settings\$$saver_class\" />\n");
    $write_files{"$xml_dir/${saver_underscore}_dream.xml"} = $dream;

    my $wallpaper = ("<wallpaper xmlns:android=\"" .
                       "http://schemas.android.com/apk/res/android\"\n" .
                     "  android:settingsActivity=\"" .
                     "$package.gen.Settings\$$saver_class\"\n" .
                     "  android:thumbnail=\"\@drawable/${saver_underscore}\" />\n");
    $write_files{"$xml_dir/${saver_underscore}_wallpaper.xml"} = $wallpaper;

    $daydream_java .=
      ("  public static class $saver_class extends XScreenSaverDaydream {\n" .
       "  }\n" .
       "\n");

    $wallpaper_java .=
      ("  public static class $saver_class extends XScreenSaverWallpaper {\n" .
       "  }\n" .
       "\n");

    $settings_java .=
      ("  public static class $saver_class extends XScreenSaverSettings\n" .
       "    implements SharedPreferences.OnSharedPreferenceChangeListener {\n" .
       "  }\n" .
       "\n");

    $fntable_h2 .= ",\n  " if $fntable_h2 ne '';
    $fntable_h3 .= ",\n  " if $fntable_h3 ne '';

    $fntable_h2 .= "${saver}_xscreensaver_function_table";
    $fntable_h3 .= "{\"${saver}\", &${saver}_xscreensaver_function_table, " .
                     'API_' . ($gl_p ? 'GL' : 'XLIB') . '}';
  }

  $arrays =~ s/^/  /gm;
  $arrays = ($xml_header .
             "<resources xmlns:xliff=\"" .
             "urn:oasis:names:tc:xliff:document:1.2\">\n" .
             $arrays .
             "</resources>\n");

  $strings =~ s/^/  /gm;
  $strings = ($xml_header .
              "<resources>\n" .
              $strings .
              "</resources>\n");

  $manifest .= "<activity android:name=\"$package.XScreenSaverSettings\" />\n";

  $manifest .= ("<activity android:name=\"" .
                "org.jwz.xscreensaver.XScreenSaverActivity\"\n" .
                "  android:theme=\"\@android:style/Theme.Holo\"\n" .
                "  android:label=\"\@string/app_name\">\n" .
                "  <intent-filter>\n" .
                "    <action android:name=\"android.intent.action" .
                ".MAIN\" />\n" .
                "    <category android:name=\"android.intent.category" .
                ".LAUNCHER\" />\n" .
                "  </intent-filter>\n" .
                "  <intent-filter>\n" .
                "    <action android:name=\"android.intent.action" .
                ".VIEW\" />\n" .
                "    <category android:name=\"android.intent.category" .
                ".DEFAULT\" />\n" .
                "    <category android:name=\"android.intent.category" .
                ".BROWSABLE\" />\n" .
                "  </intent-filter>\n" .
                "</activity>\n");


  $manifest .= ("<activity android:name=\"" .
                "org.jwz.xscreensaver.XScreenSaverTVActivity\"\n" .
                "  android:theme=\"\@android:style/Theme.Holo\"\n" .
                "  android:label=\"\@string/app_name\">\n" .
                "  <intent-filter>\n" .
                "    <action android:name=\"android.intent.action" .
                ".MAIN\" />\n" .
                "    <category android:name=\"android.intent.category" .
                ".LEANBACK_LAUNCHER\" />\n" .
                "  </intent-filter>\n" .
                "  <intent-filter>\n" .
                "    <action android:name=\"android.intent.action" .
                ".VIEW\" />\n" .
                "    <category android:name=\"android.intent.category" .
                ".DEFAULT\" />\n" .
                "    <category android:name=\"android.intent.category" .
                ".BROWSABLE\" />\n" .
                "  </intent-filter>\n" .
                "</activity>\n");




  # Android wants this to be an int
  my $versb = $vers;
  $versb =~ s/^(\d+)\.(\d+).*$/{ $1 * 10000 + $2 * 100 }/sex;
  $versb++ if ($versb == 53500); # Herp derp

  $manifest =~ s/^/   /gm;
  $manifest = ($xml_header .
               "<manifest xmlns:android=\"" .
               "http://schemas.android.com/apk/res/android\"\n" .
               "  package=\"$package\"\n" .
               "  android:versionCode=\"$versb\"\n" .
               "  android:versionName=\"$vers\">\n" .

               "  <uses-sdk android:minSdkVersion=\"14\"" .
               " android:targetSdkVersion=\"19\" />\n" .

               "  <uses-feature android:glEsVersion=\"0x00010001\"\n" .
               "    android:required=\"true\" />\n" .

               "  <uses-feature android:name=\"android.software.leanback\"\n" .
               "    android:required=\"false\" />\n" .

               "  <uses-feature" .
               " android:name=\"android.hardware.touchscreen\"\n" .
               "    android:required=\"false\" />\n" .

               "  <uses-permission android:name=\"" .
                   "android.permission.INTERNET\" />\n" .
               "  <uses-permission android:name=\"" .
                   "android.permission.READ_EXTERNAL_STORAGE\" />\n" .

               "  <application android:icon=\"\@drawable/thumbnail\"\n" .
               "    android:banner=\"\@drawable/thumbnail\"\n" .
               "    android:label=\"\@string/app_name\"\n" .
               "    android:name=\".XScreenSaverApp\">\n" .
               $manifest .
               "  </application>\n" .
               "</manifest>\n");

  $daydream_java = ("package org.jwz.xscreensaver.gen;\n" .
                    "\n" .
                    "import org.jwz.xscreensaver.XScreenSaverDaydream;\n" .
                    "import org.jwz.xscreensaver.jwxyz;\n" .
                    "\n" .
                    "public class Daydream {\n" .
                    $daydream_java .
                    "}\n");

  $wallpaper_java = ("package org.jwz.xscreensaver.gen;\n" .
                     "\n" .
                     "import org.jwz.xscreensaver.XScreenSaverWallpaper;\n" .
                     "import org.jwz.xscreensaver.jwxyz;\n" .
                     "\n" .
                     "public class Wallpaper {\n" .
                     $wallpaper_java .
                     "}\n");

  $settings_java = ("package org.jwz.xscreensaver.gen;\n" .
                    "\n" .
                    "import android.content.SharedPreferences;\n" .
                    "import org.jwz.xscreensaver.XScreenSaverSettings;\n" .
                    "\n" .
                    "public class Settings {\n" .
                    $settings_java .
                    "}\n");

  $write_files{"$project_dir/AndroidManifest.xml"}     = $manifest;
  $write_files{"$values_dir/settings.xml"} = $arrays;
  $write_files{"$values_dir/strings.xml"}  = $strings;
  $write_files{"$java_dir/Daydream.java"}  = $daydream_java;
  $write_files{"$java_dir/Wallpaper.java"} = $wallpaper_java;
  $write_files{"$java_dir/Settings.java"}  = $settings_java;

  my $fntable_h = ("extern struct xscreensaver_function_table\n" .
                   "  " . $fntable_h2 . ";\n" .
                   "\n" .
                   "static const struct function_table_entry" .
                   " function_table[] = {\n" .
                   "  " . $fntable_h3 . "\n" .
                   "};\n");
  $write_files{"$gen_dir/function-table.h"} = $fntable_h;


  $write_files{"$values_dir/attrs.xml"} =
    # This file doesn't actually have any substitutions in it, so it could
    # just be static, somewhere...
    # SliderPreference.java refers to this via "R.styleable.SliderPreference".
    ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" .
     "<resources>\n" .
     "  <declare-styleable name=\"SliderPreference\">\n" .
     "    <attr name=\"android:summary\" />\n" .
     "  </declare-styleable>\n" .
     "</resources>\n");


  foreach my $file (sort keys %write_files) {
    my ($dir) = ($file =~ m@^(.*)/[^/]*$@s);
    system ("mkdir", "-p", $dir) if (! -d $dir && !$debug_p);
    my $body = $write_files{$file};
    $body = "// Generated by $progname\n$body"
      if ($file =~ m/\.(java|[chm])$/s);
    write_file_if_changed ($file, $body);
  }

  # Unlink any .xml files from a previous run that shouldn't be there:
  # if a hack is removed from $ANDROID_HACKS in android/Makefile but
  # the old XML files remain behind, the build blows up.
  #
  foreach my $dd ($xml_dir, $gen_dir, $java_dir) {
    opendir (my $dirp, $dd) || error ("$dd: $!");
    my @files = readdir ($dirp);
    closedir $dirp;
    foreach my $f (sort @files) {
      next if ($f eq '.' || $f eq '..');
      $f = "$dd/$f";
      next if (defined ($write_files{$f}));
      if ($f =~ m/_(settings|wallpaper|dream)\.xml$/s ||
          $f =~ m/(Settings|Daydream)\.java$/s) {
        print STDERR "$progname: rm $f\n";
        unlink ($f) unless ($debug_p);
      } else {
        print STDERR "$progname: warning: unrecognised file: $f\n";
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
  print STDERR "usage: $progname [--verbose] [--debug]" .
    " [--build-android] files ...\n";
  exit 1;
}

sub main() {
  my $android_p = 0;
  my @files = ();
  while ($#ARGV >= 0) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/) { $verbose++; }
    elsif (m/^-v+$/) { $verbose += length($_)-1; }
    elsif (m/^--?debug$/s) { $debug_p++; }
    elsif (m/^--?build-android$/s) { $android_p++; }
    elsif (m/^-./) { usage; }
    else { push @files, $_; }
#    else { usage; }
  }

  usage unless ($#files >= 0);
  my $failures = 0;
  foreach my $file (@files) {
    $failures += check_config ($file);
  }

  build_android (@files) if ($android_p);

  exit ($failures);
}

main();
