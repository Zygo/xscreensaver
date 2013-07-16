#!/usr/bin/perl -w
# Copyright © 2008-2012 Jamie Zawinski <jwz@jwz.org>
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
# Created:  1-Aug-2008.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my $version = q{ $Revision: 1.5 $ }; $version =~ s/^[^\d]+([\d.]+).*/$1/;

my $verbose = 0;


my $xlockmore_default_opts = '';
foreach (qw(count cycles delay ncolors size font)) {
  $xlockmore_default_opts .= "{\"-$_\", \".$_\", XrmoptionSepArg, 0},\n";
}
$xlockmore_default_opts .= 
 "{\"-wireframe\", \".wireframe\", XrmoptionNoArg, \"true\"},\n" .
 "{\"-3d\", \".use3d\", XrmoptionNoArg, \"true\"},\n";

my $analogtv_default_opts = '';
foreach (qw(color tint brightness contrast)) {
  $analogtv_default_opts .= "{\"-tv-$_\", \".TV$_\", XrmoptionSepArg, 0},\n";
}



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

  $file = "glx/$file" unless (-f $file);
  my $body = '';
  local *IN;
  open (IN, "<$file") || error ("$file: $!");
  while (<IN>) { $body .= $_; }
  close IN;
  $file =~ s@^.*/@@;

  my $xlockmore_p = 0;
  my $analogtv_p = ($body =~ m/ANALOGTV_DEFAULTS/);

  $body =~ s@/\*.*?\*/@@gs;
  $body =~ s@^#\s*(if|ifdef|ifndef|elif|else|endif).*$@@gm;
  $body =~ s/ANALOGTV_(DEFAULTS|OPTIONS)//gs;

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
  $switch_to_res{-fps}  = 'doFPS: true';

  my ($ign, $opts) = ($body =~ m/(_options|\bopts)\s*\[\]\s*=\s*{(.*?)}\s*;/s);
  if  ($xlockmore_p || $analogtv_p || $opts) {
    $opts = '' unless $opts;
    $opts .= ",\n$xlockmore_default_opts" if ($xlockmore_p);
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
sub parse_xml($$) {
  my ($saver, $switch_to_res) = @_;
  my $file = "config/" . lc($saver) . ".xml";
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

  print STDERR "$progname: $file: options:\n" if ($verbose > 2);
  foreach (split (m/\001/, $body)) {
    next if (m/^\s*$/s);
    my ($type, $args) = m@^<([?/]?[-_a-z]+)\b\s*(.*)$@si;
    error ("$progname: $file: unparsable: $_") unless $type;
    next if ($type =~ m@^/@);

    if ($type =~ m/^([hv]group|\?xml|command|string|file|_description|xscreensaver-(image|text))/s) {

    } elsif ($type eq 'screensaver') {
      my ($name) = ($args =~ m/\b_label\s*=\s*\"([^\"]+)\"/);
      my $val = "progclass = $name";
      push @result, $val;
      print STDERR "$progname: $file:   name:    $name\n" if ($verbose > 2);

    } elsif ($type eq 'number') {
      my ($arg) = ($args =~ m/\barg\s*=\s*\"([^\"]+)\"/);
      my ($val) = ($args =~ m/\bdefault\s*=\s*\"([^\"]+)\"/);
      $val = '' unless defined($val);

      my $switch = $arg;
      $switch =~ s/\s+.*$//;
      my ($res) = $switch_to_res->{$switch};
      error ("$file: no resource for $type switch \"$arg\"") unless $res;
      $res =~ s/: \%$//;
      error ("$file: unparsable value: $res") if ($res =~ m/:/);
      $val = "$res = $val";
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
          $val = $val2 unless ($val2 eq '%');

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

    } else {
      error ("$file: unknown type \"$type\" for no arg");
    }
  }

  return @result;
}


sub check_config($) {
  my ($saver) = @_;

  # kludge
  return 0 if ($saver =~ m/(-helper)$/);

  my ($src_opts, $switchmap) = parse_src ($saver);
  my (@xml_opts) = parse_xml ($saver, $switchmap);

  my $failures = 0;
  foreach my $claim (@xml_opts) {
    my ($res, $compare, $xval) = ($claim =~ m/^(.*) (=|!=) (.*)$/s);
    error ("$saver: unparsable xml claim: $_") unless $compare;

    my $sval = $src_opts->{$res};
    if ($res =~ m/^TV/) {
      print STDERR "$progname: $saver: OK: skipping \"$res\"\n"
        if ($verbose > 1);
    } elsif (!defined($sval)) {
      print STDERR "$progname: $saver: $res: not in source\n";
    } elsif ($compare eq '!='
             ? $sval eq $xval
             : $sval ne $xval) {
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
  foreach (@files) { $failures += check_config($_); }
  exit ($failures);
}

main();
