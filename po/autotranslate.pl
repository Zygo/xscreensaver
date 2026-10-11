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
# Fill in untranslated strings using Translate Shell:
# https://www.soimort.org/translate-shell/
#
# These translations are probably not great, but probably better than nothing.
# And "nothing" was what we had: most languages hadn't been updated since 2002.
#
# To reduce the chance of errors in the auto-translations:
#
#   - Old translations that were marked "fuzzy" were re-translated,
#     because "fuzzy" usually means "quite wrong".
#
#   - If two different English phrases translate to the same text,
#     then both translations are considered invalid and omitted.
#
#   - Error checking is done on printf format strings, to ensure
#     that the translated string has the same ones in the same order.
#
#   - All numbers within strings are left untranslated.
#
# Created:  8-Sep-2026.

require 5;
use diagnostics;
use strict;

use POSIX;
use IPC::Open3;
use Time::HiRes qw(sleep time gettimeofday);

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.06 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 1;
my $debug_p = 0;
my $load_delay = 0.2;	# Seconds delay before each API call
my $write_every = 20;	# Flush our changes to the .po file regularly

my @trans_cmd = ('trans',
                 '-s', 'en-US',
                 '-brief',
                 '-no-ansi',
                 '-no-autocorrect',
                 '-no-browser');

sub blurb() {
  return ("$progname: " . strftime('%H:%M:%S', localtime) . ': ');
}


# Duplicated in hacks/check-configs.pl and OSX/build-locales.pl
sub unquote_c_string($) {
  my ($str) = @_;

  $str =~ s/\\\"/\001/gs;		   # Protect literal \"
  $str = join('', ($str =~ m/"(.*?)"/g));  # "ABC\n" "DEF\n" => "ABC\nDEF\n"
  $str =~ s/\001/\"/gs;

  $str =~ s/\\n/\n/gs;
  $str =~ s/\\t/\t/gs;
  $str =~ s/\\([-\"\'])/$1/gs;		   # Other backslashes
  error ("mystery backslashes: $str") if ($str =~ m/\\/s);
  return $str;
}

sub quote_c_string($) {
  my ($str) = @_;
  $str =~ s/\n/\\n/gs;		# quote \n
  $str =~ s/\t/\\t/gs;
  $str =~ s/([\"])/\\$1/gs;	# quote "
  $str =~ s/(\\n)/$1"\n"/gs;	# "A\nB" => "A\n" RET "B"
  $str = "\"$str\"";
  $str =~ s/\"\n\"\"/\"/s;	# Trailing ""
  return $str;
}


sub pipe_exec(@) {
  my (@cmd) = @_;

  my $av0 = $cmd[0];
  my ($inf, $outf, $errf);
  $errf = Symbol::gensym;
  print STDERR blurb() . "exec: @cmd\n" if ($verbose > 3);
  my $pid = open3 ($inf, $outf, $errf, @cmd);
  close ($inf);
  local $/ = undef;  # read entire file

  my $val = '';
  my $errs = '';
  while (<$outf>) { $val .= $_; }
  while (<$errf>) { $errs .= $_; }
  waitpid ($pid, 0);

  my $exit_value  = $? >> 8;
  my $signal_num  = $? & 127;
  my $dumped_core = $? & 128;

  if ($errs) {
    utf8::decode ($errs);   # Parse multi-byte UTF-8 into wide chars.
    print STDERR blurb() . "$av0: " .
          join ("\n" . blurb() . "$av0: ", split("\n", $errs)) . "\n";
  }

  $exit_value = -1 if (!$exit_value &&
                       $errs !~ m/^\s*$/s &&
                       $val  =~ m/^\s*$/s);

  # When we hit the rate limit, that seems to last about an hour?
  #
  $load_delay += 30 if ($errs =~ m/rate limit/si);
  $load_delay = 60*5 if ($load_delay > 60*5);

  error ("$av0: core dumped!") if ($dumped_core);
  error ("$av0: signal $signal_num!") if ($signal_num);
  error ("$av0: exited with $exit_value!") if ($exit_value);

  utf8::decode ($val);   # Parse multi-byte UTF-8 into wide chars.
  return $val;
}


sub pipe_exec_retry(@) {
  my (@cmd) = @_;
  my $retries = 100;
  my $val = '';

  for (my $i = 0; $i < $retries; $i++) {
    sleep ($load_delay);
    eval {
      $val = pipe_exec (@cmd);
      $load_delay = 0.2;  # Success: reset
    };
    return $val if ($val && !$@);
    print STDERR blurb() . "retrying in $load_delay seconds...\n"
      if ($load_delay >= 60 ||
          ($verbose && ($load_delay >= 5 || $verbose > 2)));
    $load_delay += 2;
  }
  error ($@ . " (after $retries tries)");
}


sub translate($$) {
  my ($lang, $key) = @_;

  # As of 2026, Google Translate does not support Walon.
  return $key if ($lang eq 'wa');

  error ("contains backslashes: $key") if ($key =~ m/\\/s);

  # Split the key into paragraphs and translate them separately,
  # preserving vertical whitespace.
  #
  my @paras = split (/(\s*\n\s*\n\s*)/s, $key);
  error ("blank: $key") unless @paras;

  my @val = ();

  foreach my $para (@paras) {

    # Sometimes the translator screws up format directives, turning "%-13s"
    # into nonsensical stuff like "%-13 с".  So sectionize each line into
    # before and after format codes, and don't translate those.  This may
    # result in bad translations when "%d foo" would be better translated as
    # "bar %d", but, alas.
    #
    # We also need to not translate URLs.

    my @secs = ();
    foreach my $sec (split (/ ( \s* (?: % [-.,\da-z]* |
                                        \b https?: [^\s()]+
                                    ) \s* ) /six,
                            $para)) {

      if ($sec =~ m/^\s*$/s ||				# Blank
          $sec =~ m/^[^a-z]*$/si ||			# No letters
          $sec =~ m/^[-\s\d.,\%\$]+$/s ||		# Only digits
          $sec =~ m/^\s*%[-.]*\d*[a-z]+[.,;]?\s*$/si ||	# %.100s %-13s
          $sec =~ m/^\s*(https?:[^\s]+\s*)+$/si ||	# URL only
          0) {
        push @secs, $sec;
        next;						# Skip
      }

      my ($white1, $txt, $white2) = ($sec =~ m/^(\s*)(.*?)(\s*)$/s);
      my $otxt = $txt;

      # Underscores used to indicate kbd shortcuts screw up translations.
      $txt =~ s/_//s;
      $txt = 'Ok' if ($txt eq 'OK');	# Avoid all-caps result like "D'ACORD"

      my $txt2 = $txt;
      $txt2 =~ s@^([-/:])@\\$1@s;	# Don't interpret text as commands
      my $lang_arg = ':' . $lang;
      $lang_arg =~ s/_/-/gs;
      my @cmd = ( @trans_cmd, $lang_arg, $txt2);

      my $val = pipe_exec_retry (@cmd);
      error ("no translation: \"$key\"") unless $val;

      $val =~ s/^\s+|\s+$//gs;
      $val =~ s/\\//gs;

      $val = $otxt if ($val eq $txt);  # Unchanged: keep old case/underscores.

      $val = $white1 . $val . $white2;

      my ($key2, $val2) = ($sec, $val);
      foreach ($key2, $val2) { s/\n/\\n/gs; s/\t/\\t/gs; }
      print STDERR blurb() . "\"$key2\"\n\t\t\t => \"$val2\"\n"
        if ($verbose > 2);

      push @secs, $val;
    }

    push @val, join ('', @secs);
  }

  my $val = join ('', @val);
  my ($key2, $val2) = ($key, $val);
  foreach ($key2, $val2) { s/\n/\\n/gs; }
  print STDERR blurb() . "\"$key2\"\n\t\t\t => \"$val2\"\n"
    if ($verbose);
  return $val;
}


sub write_if_changed($$$;$) {
  my ($file, $body, $obody, $vers) = @_;

  if ($body eq $obody) {
    print STDERR blurb() . "$file unchanged\n" if ($verbose > 1);
    return;
  }

  my $date = strftime ('%Y-%m-%d %H:%M%z', localtime());
  my ($oy) = ($body =~ m/PO-Revision-Date:\s*(\d{4})\b/si);
  error ("$file: no rev year") unless ($oy);
  ($body =~ s/"(PO-Revision-Date:).*?"/"$1 $date\\n"/gm) ||
    error ("$file: unable to update date");
  $date = strftime ('%Y-%m-%d', localtime());

  error ("no version") unless $vers;
  ($body =~ s/"(Project-Id-Version:).*?"/"$1 XScreenSaver $vers\\n"/gm) ||
    error ("$file: unable to update version");

  ($body =~ s/"(X-Generator:).*?"/"$1 $progname $version\\n"/gm) ||
    error ("$file: unable to update X-Generator");

  if ($file !~ '\bwa\.po$' &&
      $body !~  m/# Automatic translations added by jwz/si) {
    my $blurb = ("#\n" .
                 "# Automatic translations added by jwz, $date" .
                 " (previous update: $oy)\n" .
                 "#\n" .
                 "# NOTE TO TRANSLATORS:\n" .
                 "#\n" .
                 "#     For each entry marked \"auto-translated\", please:\n" .
                 "#\n" .
                 "#       - Verify it or correct it\n" .
                 "#       - Remove the \"auto-translated\" comment\n" .
                 "#       - Send the new file to jwz\@jwz.org\n" .
                 "#\n" .
                 "#   Thank you!\n" .
                 "#\n");
    ($body =~ s/^((#[^\n]*\n)+)/$1$blurb/s) ||
      error ("$file: unable to add blurb");
  }

  if ($debug_p) {
    my $tmp1 = sprintf ("/tmp/atr-1-%08x", rand() * 0xFFFFFFFF);
    my $tmp2 = sprintf ("/tmp/atr-2-%08x", rand() * 0xFFFFFFFF);

    $obody =~ s/(\n\n)\n+/$1/gs;	# Triple spacing

    open (my $out1, '>:utf8', $tmp1) || error ("$tmp1: $!");
    print $out1 $obody;
    close $out1;

    open (my $out2, '>:utf8', $tmp2) || error ("$tmp2: $!");
    print $out2 $body;
    close $out2;

    system ("diff -U4 $tmp1 $tmp2");
    unlink ($tmp1);
    unlink ($tmp2);
  } else {
    open (my $out, '>:utf8', $file) || error ("$file: $!");
    print $out $body;
    close $out;
    print STDERR blurb() . "wrote $file\n";
  }
}


sub uniq(@) {
  my %seen;
  grep !$seen{$_}++, @_;
}

# A key to determine whether two English strings are close enough to
# equivalent that them having identical translations seems reasonable.
#
sub dup_sortkey($) {
  my ($str) = @_;
  $str =~ s/"\n"//gs;
  $str =~ s/_//gs;
  $str =~ s/[-_\s.,?!]+/ /gs;
  $str =~ s/^\s+|\s+$//gs;

  # De-pluralize and de-abbreviate some things.
  $str =~ s/\b(sec)(s?)\b/second$2/gsi;
  $str =~ s/\b(min)(s?)\b/minute$2/gsi;
  $str =~ s/\bmax\b/maximum/gsi;
  $str =~ s/\bgray\b/grey/gsi;
  $str =~ s/\bthe (date)\b/$1/gsi;
  $str =~ s/\b(hour|minute|second|gradient|transition|color|column|eye|
               explosion|trail|star|mountain|influencer)s?\b/$1/gsix;
  $str =~ s/(state|republic) of (the )?//gsi;
  $str =~ s/ (city|republic)$//gsi;
  $str =~ s/ D C$/ DC/gsi;
  $str =~ s/\b(covid)[-\s\d]*\b/$1/gsi;

  $str =~ s/\b(quick|quickly)\b/fast/gsi;
  $str =~ s/\b(spin rate)\b/spin speed/gsi;
  $str =~ s/\b(turn speed)\b/rotation speed/gsi;
  $str =~ s/anti[- ]?alias(ed)?/smooth/gsi;
  $str =~ s/\b(revolution|rotation)\b/spin/gsi;
  $str =~ s/^number of (.*)/$1 count/gsi;
  $str =~ s/^pair of (glasses)/$1/gsi;
  $str =~ s/\b(tiny)\b/small/gsi;
  $str =~ s/\b(huge)\b/large/gsi;
  $str =~ s/\b(melty)\b/melt/gsi;
  $str =~ s/springiness/flexibility/gsi;
  $str =~ s/goggles/glasses/gsi;
  $str =~ s/seldom/rarely/gsi;
  $str =~ s/smoothed/smooth/gsi;

  return lc($str);
}


sub autotranslate($) {
  my ($file) = @_;

  my $body = '';
  open (my $in, '<:utf8', $file) || error ("$file: $!");
  print STDERR blurb() . "reading $file...\n" if ($verbose > 1);
  local $/ = undef;  # read entire file
  while (<$in>) { $body .= $_; }
  close $in;
  my $obody = $body;

  my ($lang) = ($body =~ m/^\"Language:\s*([a-z_]+)\s*(\\n)*"/mi);
  error ("$file: no language specified") unless ($lang);

  my $vers = '';
  if (open (my $in, '<:utf8', '../utils/version.h')) {
    local $/ = undef;  # read entire file
    $vers = <$in>;
    close $in;
    ($vers) = ($vers =~ m/XSCREENSAVER_VERSION "(.*?)"/s);
  }
  error ("no version") unless $vers;


  # Point to the language's Wikipedia site?
  # But most of those pages are 404.
  # $body =~ s/\b(en)(\.wikipedia\.org)\b/$lang$2/gs;

  $body =~ s/\\+(\\\")/$1/gs;  # WTF
  $body =~ s/\\([-.,])/$1/gs;

  # Sectionize the file so that we can do N strings at a time,
  # saving the file in between.

  my @secs = split (/\n\n/, $body);

  my %reverse;  # Track translations that are many-to-1, which is a bad sign.

  my $total = 0;
  my $auto_count = 0;
  my $manual_count = 0;
  my $untranslated_count = 0;

  my $tick = 0;
  foreach my $sec (@secs) {

    $sec =~ s/\\\"/\001/gs;  # Protect literal \"

    $sec =~ s@( (?: \# [^\n]* \n )+ )
              msgid  \s+ ( (?: "[^\"]* " \s* )+ ) \s* \n
              msgstr \s+ ( (?: "[^\"]* " \s* )+ ) \s* \n? @{
      my ($comm, $msgid, $msgstr) = ($1, $2, $3);
  
      foreach ($comm, $msgid, $msgstr) { s/\001/\\\"/gs; }

      my $msgid2  = unquote_c_string ($msgid);
      my $msgstr2 = unquote_c_string ($msgstr);
  
      $comm =~ s/--/-\\-/gs;  # intltool-update wants "-\-" for some reason

      my $msgid3  = $msgid2;
      my $msgstr3 = $msgstr2;
      foreach ($msgid3, $msgstr3) {
        s/^\s*//gs;
        s/\n.*/.../gs;
        s/\s+/ /gs;
        s/^"(.*)"$/$1/gs;
      }
  
      # $msgid2 = "" if ($total > 10);  # Debug
  
      error ("contains backslashes: $msgid2") if ($msgid2 =~ m/\\/s);
      error ("contains backslashes: $msgstr2") if ($msgstr2 =~ m/\\/s);

     # Force retranslation of untranslated strings
     if (0) { ####
        if (dup_sortkey($msgid2) eq dup_sortkey($msgstr2)) {
        print STDERR blurb() . "force retranslate untranslated: $msgid3\n";
        $msgstr2 = "";
        }
      }

      # Force retranslation of ancient things that were marked "fuzzy".
      if ($msgstr2 && $comm =~ m/\bfuzzy\b/) {
        print STDERR blurb() . "force retranslate fuzzy: $msgid3\n";
        $msgstr2 = "";
      }

      my $skip_p = ($msgid2 eq "");

      # Sometimes translating this fails.
      $skip_p = 1 if ($msgid2 =~ m/^FPS:/si);

      if ($skip_p) {
      } elsif ($msgstr2 ne "") {
        print STDERR blurb() . "$file: already translated: \"$msgid3\"\n"
          if ($verbose > 2);
      } else {
        print STDERR blurb() . "$file: translating: \"$msgid3\"\n"
          if ($verbose > 2);
        $msgstr2 = translate ($lang, $msgid2);
        $msgstr = quote_c_string ($msgstr2);
        $msgstr .= "\n";

        $comm =~ s/^\#,[^\n]*\n//gm;  # Omit "fuzzy", etc.

        if ($msgid2 eq $msgstr2) {  # Wasn't translated at all
          $comm =~ s/^\# \(auto-translated\)\n//gm;
        } elsif (! ($comm =~ m/auto-translated/si)) {
          $comm = "# (auto-translated)\n" . $comm;
        }

        $tick++;
      }
  
      # Make sure format directives didn't get broken in translation.
      # Check that the same directives exist in both strings.
      if ($msgid2 ne "") {
        my $fmt1 = join (" ", ($msgid2  =~ m/(%\.*[^\s\"\'\\(),.:\/\177-\x{ffffff}]*)/g));
        my $fmt2 = join (" ", ($msgstr2 =~ m/(%\.*[^\s\"\'\\(),.:\/\177-\x{ffffff}]*)/g));
        my ($msgid2b, $msgstr2b) = ($msgid2, $msgstr2);
        foreach ($msgid2b, $msgstr2b) { s/\n/\\n/gs; }
        $fmt2 =~ s/Uruchomi//gs; # pl.po
        error ("$lang: format screwup:\n" .
		      "\t\t  \"$fmt1\" != \"$fmt2\";\n" .
		      "\t\t  \"$msgid2b\"\n\t\t\t => \"$msgstr2b\"")
          unless ($fmt1 eq $fmt2);
      }

      # For each localized string, maintain a list of all the English
      # strings it was translated from, to detect bad auto-translations.
      if ($msgid2 ne "") {
        my $dk = $msgstr2;
        $dk =~ s/\s+//gs;
        $dk =~ s/^\s+|\s+$//gs;
        my %o;
        %o = %{$reverse{$dk}} if ($reverse{$dk});
        $o{dup_sortkey($msgid2)} = $msgid2;
        $reverse{$dk} = \%o;
      }

      if ($msgid2 eq $msgstr2) {
        $untranslated_count++;
      } elsif ($comm =~ m/auto-translated/s) {
        $auto_count++;
      } else {
        $manual_count++;
      }

      "${comm}msgid $msgid\nmsgstr $msgstr\n";
    }@gsexi;

    $total += $tick;

    if ($tick >= $write_every) {
      print STDERR blurb() . "$file: translated $tick strings\n"
        if ($verbose);
      $body = join ("\n\n", @secs);
      $body =~ s/(\n\n)\n+/$1/gs;		# Triple spacing
      write_if_changed ($file, $body, $obody, $vers);
      $obody = $body;
      $tick = 0;
    }
  }

  $body = join ("\n\n", @secs);


  # Make a second pass through to un-translate the collisions.
  #
  my $collision_count = 0;

  $body =~ s/\\\"/\001/gs;  # Protect literal \"

  $body =~ s@( (?: \# [^\n]* \n )+ )
            msgid  \s+ ( (?: "[^\"]* " \s* )+ ) \s* \n
            msgstr \s+ ( (?: "[^\"]* " \s* )+ ) \s* \n? @{
    my ($comm, $msgid, $msgstr) = ($1, $2, $3);

    foreach ($comm, $msgid, $msgstr) { s/\001/\\\"/gs; }

    my $msgid2  = unquote_c_string ($msgid);
    my $msgstr2 = unquote_c_string ($msgstr);

    if ($msgid2 ne "") {
      my $dk = $msgstr2;
      $dk =~ s/\s+//gs;
      $dk =~ s/^\s+|\s+$//gs;
      error ("unset: $dk") unless ($reverse{$dk});
      my %o = %{$reverse{$dk}};
      my \@misses = ();
  
      foreach my $oid (sort keys %o) {
        my $oid2 = $o{$oid};
        next if ($oid eq dup_sortkey ($msgid2));
        push \@misses, $oid2;
      }
  
      if (\@misses > 1) {
        # Untranslate it, and leave a comment saying why.
        $comm =~ s/^\# \(auto-translated\)\n//gm;
        $comm =~ s/^\# auto: untranslated: [^\n]+\n//gm;
        $comm = "# auto: untranslated: \"$msgstr2\" also used for \"" .
                join ('", "', \@misses) . "\"\n" .
                $comm;
        $msgstr = $msgid;
      }
  
      $collision_count++ if ($comm =~ m/auto: untranslated/s);
    }

    "${comm}msgid $msgid\nmsgstr $msgstr\n\n";
  }@gsexi;


  print STDERR blurb() . "$file:" .
                         " manual = $manual_count;" .
                         " auto = $auto_count;" .
                         " untranslated = $untranslated_count;" .
                         " collisions = $collision_count\n"
    if ($verbose);

  # Remove the old, dead strings from the end
  $body =~ s/(\#([~,][^\n]*| \(auto-translated\))\n+)+$//s;

  $body =~ s/\s+$//s;
  $body .= "\n";
  $body =~ s/(\n\n)\n+/$1/gs;		# Triple spacing

  write_if_changed ($file, $body, $obody, $vers);
}


sub which($) {
  my ($prog) = @_;
  foreach my $d (split (/:/, $ENV{PATH})) {
    my $path = "$d/$prog";
    if (-x $path) {
      return $path;
    }
  }
  return undef;
}


sub error($) {
  my ($err) = @_;
  die $err . "\n";  # Caught in main
}

sub usage() {
  print STDERR "usage: $progname [--verbose] [--quiet] files ...\n";
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
    elsif (m/^--?q(uiet)?$/s) { $verbose = 0; }
    elsif (m/^--?debug$/s) { $debug_p++; }
    elsif (m/^-./s) { usage; }
    else { push @files, $_; }
#   else { usage; }
  }

  eval {
  
    usage unless (@files);
  
    which ($trans_cmd[0]) ||
      error ('"' . $trans_cmd[0] . '" is not installed');
  
    foreach my $f (@files) { autotranslate ($f); }
  };
  if ($@) {   # error() called and un-caught.
    print STDERR blurb() . "$@\n";
    exit 1;
  }
}

main();
exit 0;
