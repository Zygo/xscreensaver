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
#   1: Download "Admin 0 - Countries" and "Admin 1 - States, Provinces" from
#      https://www.naturalearthdata.com/downloads/50m-cultural-vectors/
#      (The lower resolution 110m data set also works and is much smaller.)
#
#      https://www.naturalearthdata.com/http//www.naturalearthdata.com/download/50m/cultural/ne_50m_admin_0_countries_lakes.zip
#      https://www.naturalearthdata.com/http//www.naturalearthdata.com/download/50m/cultural/ne_50m_admin_1_states_provinces_lakes.zip
#
#   2: Drag those two zips onto https://mapshaper.org/
#
#   3: Export as GeoJSON and unzip the downloaded file to get:
#      ne_50m_admin_0_countries.json
#      ne_50m_admin_1_states_provinces.json
#
#   4: Run this script with those two .json files to create countries.c
#
# Created:  4-Feb-2026.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.13 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 1;

use POSIX;
use JSON::Any;
use LWP::UserAgent;
use utf8;

sub url_quote($) {
  my ($u) = @_;
  $u =~ s|([^-a-zA-Z0-9.\@_\r\n])|sprintf("%%%02X", ord($1))|ge;
  return $u;
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

sub sparql_json($$) {
  my ($ua, $query) = @_;

  $query =~ s/\s+/ /gs;
  my $url = ('https://query.wikidata.org/sparql?query=' .
             url_quote ($query) .
             '&format=json');
  my $res = $ua->get ($url);
  my $ret = ($res && $res->code) || 'null';
  error ("sparql: status $ret") unless ($res->is_success);
  return json_decode ($res->decoded_content);
}


# For some reason the country JSON includes population but the
# state JSON does not.  So let's grab it from Wikipedia.
# And grab the official size (area) while we're at it.
#
sub download_populations($) {
  my ($ua) = @_;

  # This is one of the *nastiest* query languages I've seen. Good job.
  # https://query.wikidata.org/
  # https://www.wikidata.org/wiki/Wikidata:SPARQL_tutorial
  #
  # To get article source:
  # https://en.wikipedia.org/w/api.php?action=query&format=json
  #  &prop=revisions&formatversion=2&rvprop=content&rvslots=*
  #  &titles=_TITLE_
  #
  my %pops;
  my %areas;
  my @queries = (

    # Maybe there's a way to do these as one query, but I sure can't tell.

    # US states
    'SELECT DISTINCT ?stateLabel ?population ?area
     WHERE {
       ?state wdt:P31/wdt:P279* wd:Q35657 ;
              wdt:P1082 ?population ;
              wdt:P2046 ?area .
       SERVICE wikibase:label { bd:serviceParam wikibase:language "en" }
     }
     ORDER BY ASC(?stateLabel) LIMIT 100',

    # Canadian provinces
    'SELECT DISTINCT ?stateLabel ?population ?area
     WHERE {
       ?state wdt:P31/wdt:P279* wd:Q11828004 ;
              wdt:P1082 ?population ;
              wdt:P2046 ?area .
       SERVICE wikibase:label { bd:serviceParam wikibase:language "en" }
     }
     ORDER BY ASC(?stateLabel) LIMIT 100',

    # Canadian territories
    'SELECT DISTINCT ?stateLabel ?population ?area
     WHERE {
       ?state wdt:P31/wdt:P279* wd:Q9357527 ;
              wdt:P1082 ?population ;
              wdt:P2046 ?area .
       SERVICE wikibase:label { bd:serviceParam wikibase:language "en" }
     }
     ORDER BY ASC(?stateLabel) LIMIT 100',

    # Australian territories
    'SELECT DISTINCT ?stateLabel ?population ?area
     WHERE {
       ?state wdt:P31/wdt:P279* wd:Q14192199 ;
              wdt:P1082 ?population ;
              wdt:P2046 ?area .
       SERVICE wikibase:label { bd:serviceParam wikibase:language "en" }
     }
     ORDER BY ASC(?stateLabel) LIMIT 100',
    );

  #   Commonwealth of Kentucky
  #   Commonwealth of Massachusetts
  #   Commonwealth of Pennsylvania
  #   Commonwealth of Puerto Rico / Estado Libre Asociado de Puerto Rico
  #   Commonwealth of Virginia
  #   Commonwealth of the Northern Mariana Islands
  #   Mokuʻāina o Hawaiʻi
  #
  # "The commonwealth appellation is stylistic and carries no legal or
  # political significance."


  my $i = 0;
  foreach my $query (@queries) {
    sleep (2) if ($i);
    $i++;
    print STDERR "$progname: loading populations $i...\n" if ($verbose);
    my $json = sparql_json ($ua, $query);
    foreach my $r (@{$json->{results}->{bindings}}) {
      my $name = $r->{stateLabel}->{value};
      my $pop  = $r->{population}->{value};
      my $area = $r->{area}->{value};
      $name =~ s/[^a-z ]//gsi;  # Sometimes "Hawaiʻi"
      $pops{$name} = $pop;
      $areas{$name} = $area;
    }
  }
  return ( \%pops, \%areas );
}


# Parse the names, populations and sizes out of the existing countries.c
# instead of re-running the Wikidata queries, for speed of debugging.
#
sub reuse_data() {
  my $file = 'countries.c';
  my ( %endonyms, %areas, %pops );
  open (my $in, '<:utf8', $file) || error ("$file: $!");
  while (<$in>) {
    if (m/^G .*=\{"(.*?)","(.*?)","(.*?)","(.*?)",([-\d]+),([-\d]+),/s) {
      my ($code, $name, $endonym, $endonym2, $pop, $area) =
        ($1, $2, $3, $4, $5, $6);
      $code =~ s/-/_/gs;
      $endonyms{$name} = [ $endonym, $endonym2 ];
      $endonyms{$code} = [ $endonym, $endonym2 ];
      $areas{$name}    = $area;
      $areas{$code}    = $area;
      $pops{$name}     = $pop;
      $pops{$code}     = $pop;
    }
  }
  close ($in);
  return (\%endonyms, \%areas, \%pops);
}


# The JSON includes country names translated into many languages,
# but does not indicate which of those is the primary language of
# the country.  So download that from Wikipedia.
#
sub download_endonyms($) {
  my ($ua) = @_;

  my $delay = 2;   # Avoid 429 rate limit

  my %endonyms;

  # "There are 178 parent languages on our planet with over 1000 dialects.
  # It's amazing we communicate at all.
  # Languages and dialects, with this one thing in common..."
  #
  print STDERR "$progname: loading languages...\n" if ($verbose);
  my $json = sparql_json ($ua,
   'SELECT DISTINCT ?langLabel ?code
     WHERE {
       ?lang wdt:P31/wdt:P279* wd:Q20162172 ;
             wdt:P218 ?code .
       SERVICE wikibase:label { bd:serviceParam wikibase:language "en" }
     }
     ORDER BY ASC(?code) LIMIT 5000');
  
  my %lang_codes;	# Map language names to ISO 639-1 codes
  foreach my $r (@{$json->{results}->{bindings}}) {
    my $lang = $r->{langLabel}->{value};
    my $code = $r->{code}->{value};
    $lang_codes{$lang} = uc($code);
  }

  # Get the official languages (multiple) of each country.
  #
  sleep ($delay);
  print STDERR "$progname: loading official languages...\n" if ($verbose);
  $json = sparql_json ($ua,
   'SELECT DISTINCT ?code ?countryLabel ?officialLanguageLabel
     WHERE {
       ?country wdt:P31/wdt:P279* wd:Q7275 ;
                wdt:P297 ?code ;
                wdt:P37 ?officialLanguage .
       SERVICE wikibase:label { bd:serviceParam wikibase:language "en" }
     }
     ORDER BY ASC(?code) LIMIT 5000');

  my %official_langs;
  my %official_name;
  foreach my $r (@{$json->{results}->{bindings}}) {
    my $code = $r->{code}->{value};
    my $name = $r->{countryLabel}->{value};
    my $lang = $r->{officialLanguageLabel}->{value};
    my $lc = $lang_codes{$lang};

    $official_name{$code} = $name;

    next unless $lc;
    next if ($lc eq 'EN');

    my $o = $official_langs{$code} || [];
    push @$o, $lc;
    $official_langs{$code} = $o;
  }

  $official_name{'UY'} = 'Uruguay';


  # Find the official area (size) of each country.
  #
  my %areas;
  sleep ($delay);
  print STDERR "$progname: loading areas...\n" if ($verbose);
  $json = sparql_json ($ua,
   'SELECT DISTINCT ?code ?countryLabel ?area
       WHERE {
         ?country wdt:P31/wdt:P279* wd:Q7275 ;
                  wdt:P297 ?code ;
                  wdt:P2046 ?area .
         SERVICE wikibase:label { bd:serviceParam wikibase:language "en" }
       }
       ORDER BY ASC(?code) LIMIT 5000');

  foreach my $r (@{$json->{results}->{bindings}}) {
    my $name = $r->{countryLabel}->{value};
    my $code = $r->{code}->{value};
    my $area = $r->{area}->{value};
    $areas{$name} = $area;
    $areas{$code} = $area;
  }

  # The local name of the country in its local character set does not
  # appear to be available in Wikidata, though it is in Wikipedia.
  # So load the page for each country and parse the wiki markup for it.
  #
  # "official name" P1448 sounded promising, but it returns the name in
  # every available language, e.g. Mexico's official name in Chinese is
  # the first result.  "native name" P1705 is differently-weird and bad.
  #
  if (1) {
    print STDERR "$progname: loading endonyms...\n" if ($verbose);
    foreach my $code (sort keys %official_name) {
      sleep ($delay);
  
      next if ($code =~ m/^(CA|WS|KP|KR)$/s);
  
      my $name = $official_name{$code};
  
      $name .= ' (country)' if ($code eq 'GE');
      $name = 'China' if ($code eq 'CN');
  
      my $url = ('https://en.wikipedia.org/w/api.php?' .
                 join ('&',
                       ('action=query',
                        'format=json',
                        'prop=revisions',
                        'formatversion=2',
                        'rvprop=content',
                        'rvslots=*',
                        '&titles=' . $name)));
      my $res = $ua->get ($url);
      my $ret = ($res && $res->code) || 'null';
      error ("api.php: status $ret") unless ($res->is_success);
      my $json = json_decode ($res->decoded_content);
      my $page = $json->{query}->{pages}->[0]
                   ->{revisions}->[0]
                   ->{slots}->{main}->{content};
      my $opage = $page;
  
      $page =~ s@<!--.*?-->@@gs;
  
      $page =~ s@\{\{no\s*wrap\|(.*?)\}\}@$1@gsi;
      $page =~ s/(\{\{native *name) *list(.*?\|)name1=/$1$2/gsi;
      $page =~ s/&nbsp;/ /gs;
      $page =~ s/\{\{wbr.*?\}\}//gsi;
      $page =~ s@<wbr.*?/?>@@gsi;
  
      $page =~ s/\n==.*$//s;	# Lose everything after first heading
      $page =~ s/\|\s*national_motto.*$//s;
      $page =~ s/\|\s*national_anthem.*$//s;
      $page =~ s/\|\s*anthem.*$//s;
      $page =~ s/\|\s*religion.*$//s;
      $page =~ s/\|\s*motto.*$//s;

      my ($nname) = ($page =~ m/\b native_name \s* = \s* .*? 
                                ( \{\{ ( lang | native \s* name ) \s*
                                  [^\n]+ )/six);
         ($nname) = ($page =~ m/\b name \s* = \s* .*? 
                                ( \{\{ ( lang | native \s* name )
                                  [^\n]+ )/six)
           unless ($nname);
  
      if ($nname) {
        error ("unparsable name 1: $code: < $nname >\n\n$opage")
          unless ($nname =~ m/^\{\{(Native ?name|langx?)\s*\|/si);
      }
      $nname = '' unless $nname;
  
      $nname =~ s/^\{\{(raise|resize)\|.*?\|//gsi;
  
      my ($nname2) = ($nname =~ m/ \{\{ transliteration \s*
                                   .*?
                                   \| [^\|]*?
                                   \| ( [^\|=]*? ) \}\}
                                 /six);
         ($nname2) = ($nname =~ m/ <br \s* \/?> \s*
                                   '' ( .*? ) ''
                                 /six)
           unless ($nname2);
         ($nname2) = ($nname =~ m/ <br \s* \/?> \s*
                                   \{\{ tlit \s*
                                    \| .*?
                                    \| .*?
                                   ( .+? ) \}\}
                                 /six)
           unless ($nname2);
         ($nname2) = ($nname =~ m/ <br \s* \/?> \s*
                                   \{\{ small \| \s*
                                   \{\{ IAST  \| \s*
                                   ( .+? ) \}\}
                                 /six)
           unless ($nname2);
         ($nname2) = ($nname =~ m/ \}\} \| \{\{ \s* lang \s*
                                    \| .*?
                                    \| ( .+? ) \}\}
                                 /six)
           unless ($nname2);
         ($nname2) = ($page  =~ m/ \{\{ translit(?:eration)? \s*
                                   [^<>\[\]\{\}]*
                                   \| [^\|\{\}]+
                                   \| \s* ( [^\|=<>\[\]\{\}]+ )
                                   \}\}
                                 /six)
           if ($nname && !$nname2);
      $nname2 = '' unless defined ($nname2);

      foreach ($nname, $nname2) {
        s/\|\s*\{\{\s*Native ?name//gsi;
        s@<br\s*/?>.*@@gsi;
        s@<hr\s*/?>.*@@gsi;
        s@\{\{(efn|cite|ubl).*@@gsi;
        s@<ref.*@@gsi;
      }
  
      if ($nname) {
        $nname =~ s@\{\{lang-uz[^\|]+\|@@si;
        $nname =~ s@\{\{lang-[^\|]+\|[^\|]+\|@@si;
        $nname =~ s@\|\{\{Nastaliq@@si;
        $nname =~ s@\|\{\{nq@@si;
        error ("unparsable name 2: $code: < $nname >\n\n$opage")
          unless ($nname =~ m/ \{\{ (?: Native \s* name | langx? ) \s*
                               \|   (?: [^\|]+ ) \s*
                               \|   (   [^\|]+ )/six);
        $nname = $1;
      }
  
      foreach ($nname, $nname2) {
        s/\s+/ /gs;
        s/ \(.*//gs;
        s@ / .*@@gs;
        s/[\}\s]*$//gs;
        s/^''+(.*?)''+$/$1/gs;
      }

      error ("bogus chars: $code: < $nname >\n\n$opage")
        if ($nname =~ m/[&\[\{\]\}]/s);
      error ("bogus chars 2: $code: < $nname2 >\n\n$opage")
        if ($nname2 =~ m/[&\[\{\]\}]/s);
  
      print STDERR "$progname:   $code\t$name\t\t$nname\t$nname2\n"
        if ($verbose);
      $endonyms{$code} = [ $nname, $nname2 ];
    }
  }

  # The formatting on these pages is weird and hard to parse, so harcode them.
  my %t = (
    'IN' => [ 'भारत गणराज्य', 'Bhārat Gaṇarājya' ],
    'KP' => [ '조선민주주의인민공화국', 'Chosŏnminjujuŭiinmin\'gonghwaguk' ],
    'KR' => [ '대한민국', 'Daehanminguk' ],
    'MF' => [ 'Collectivité de Saint-Martin', '' ],
    'UZ' => [ 'Ўзбекистон Республикаси', 'O‘zbekiston Respublikasi' ],
    'VA' => [ 'Status Civitatis Vaticanae', '' ],
    'WS' => [ 'Malo Saʻoloto Tutoʻatasi o Sāmoa', '' ],

    # Could use wdt:P1705 ?nativeLabel to find official names, but there are
    # only 6 of them... "The commonwealth appellation is stylistic and carries
    # no legal or political significance."
    #
    'US-HI' => [ 'Mokuʻāina o Hawaiʻi', '' ],
    'US-KY' => [ 'Commonwealth of Kentucky', '' ],
    'US-MA' => [ 'Commonwealth of Massachusetts', '' ],
    'US-PA' => [ 'Commonwealth of Pennsylvania', '' ],
    'US-VA' => [ 'Commonwealth of Virginia', '' ],
    'PR'    => [ 'Estado Libre Asociado de Puerto Rico', '' ],
    );
  foreach my $code (sort keys %t) {
    print STDERR "$progname: replacing: $code: \"" .
                 $endonyms{$code}->[0] . "\", \"" .
                 $endonyms{$code}->[1] . "\"\n"
      if ($endonyms{$code});
   $endonyms{$code} = $t{$code};
  }

  return ( \%official_langs, \%endonyms, \%areas );
}


sub build_countries($$@) {
  my ($outfile, $reuse_p, @files) = @_;

  my $out = '';
  my @geoms = ();

  my $ua = LWP::UserAgent->new;
  $ua->agent($progname);

  my $langs    = {};
  my $endonyms = {};
  my $areas    = {};
  my $areas2   = {};
  my $pops     = {};
  if ($reuse_p) {
    ($endonyms, $areas, $pops) = reuse_data();
  } else {
    ($langs, $endonyms, $areas) = download_endonyms ($ua);
    ($pops, $areas2) = download_populations ($ua);
  }

  foreach my $k (keys %$areas2) {
    $areas->{$k} = $areas2->{$k};
  }

  $out .= '/* Generated by ' . $progname . " from\n";
  foreach my $f (@files) {
    my $f2 = $f;
    $f2 =~ s@^[./]*@@s;
    $out .= "   $f2\n";
  }
  $out .= "   on " . localtime() . '
 */

#include "countries.h"

#define D static const double
#define C static const country_path
#define P static const country_polys
#define G static const country_geom

# undef _
# define _(S) S

';

  my %dups;

  foreach my $file (@files) {

    my $body = '';
    open (my $in, '<:raw', $file) || error ("$file: $!");  # Not :utf8
    print STDERR "$progname: reading $file\n" if ($verbose);
    local $/ = undef;  # read entire file
    while (<$in>) { $body .= $_; }
    close $in;

    my $json = json_decode ($body);
    error ("$file: unparsable JSON") unless $json;

    foreach my $f (@{$json->{features}}) {
      my $geom = $f->{geometry};
      my $prop = $f->{properties};
      my $name = ($prop->{FORMAL_EN} || $prop->{NAME_EN} || $prop->{name_en} ||
                  $prop->{name});
      my $code = $prop->{ISO_A2_EH} || $prop->{iso_3166_2} || 0;
      my $pop  = $prop->{POP_EST}   || 0;
      my $area = 0;  # Not available in JSON

      if ($code eq 'US-DC') {
        $name = 'Washington, D.C.';	# Weirdos
        $pop  = 689545;
        $area = 68.35;
      }

      my @langs   = @{$langs->{$code}  || []};
      my ( $endonym, $endonym2 ) = @{$endonyms->{$code} || [ "", "" ] };

      # If this is the states file, only include US and CA.
      if ($prop->{iso_a2} &&
          $prop->{iso_a2} ne 'US' &&
          $prop->{iso_a2} ne 'CA') {
        next;
      }

      if ($name eq 'Null Island') {
        $code = 'NULL';
        $pops->{$name} = $pop = 0;
      } elsif ($name eq "R'lyeh") {
        $code = 'RL';
        $pops->{$name} = $pop = -1;
      }

      # Northern Cyprus
      $code = $prop->{ADM0_A3} if ($code eq '-99');

      if (0 && ! $endonym) {
        # See if we have a translation in the JSON. (This never happens)
        foreach my $lang (@langs) {
          my $name2 = $prop->{'NAME_' . $lang};
          if ($name2) {
            $endonym = $name2;
            print STDERR "$progname: JSON: $code\t$name\t$lang\t\t$name2\n"
              if ($verbose);
            last;
          }
        }
      }

      foreach ($endonym, $endonym2) {
        s/^\s+|\s+$//s;
        s/^([a-z])/\U$1/s;	# Capitalize leading "la ".
      }

      $code =~ s/~$/2/s;
      error ("bogus code \"$code\" for \"$name\"")
        unless ($code =~ m/^([a-z]{2,3}|[a-z]{2}-[a-z\d]{2,4}|NULL)$/si);

      $code = lc($code);
      $code =~ s/-/_/gs;

      my $ocode = $code;
      if ($dups{$code}) {
       $code .= $dups{$code};
        print STDERR "$progname: dup: $ocode -> $code\n" if ($verbose > 2);
      }
      $dups{$ocode} = ($dups{$ocode} || 1) + 1;

      if (!$pop) {
        my $name2 = $name;
        $name2 =~ s/^Territory of //s;
        $pop = $pops->{$name2};
        print STDERR "$progname: no population for " . uc($code) . ", $name2\n"
          unless (defined($pop));
        $pop = 0 unless $pop;
      }
      $pop = int($pop);

      $area = $areas->{uc($code)} unless ($area);
      $area = $areas->{$name}     unless ($area);
      if (!$area) {
        my $name2 = $name;
        $name2 =~ s/^Territory of //s;
        $area = $areas->{$name2};
        print STDERR "$progname: no area for " . uc($code) . ", $name2\n"
          if ($verbose && !$area);
        $area = 0 unless $area;
      }
      $area = int($area);

      next if ($code eq 'us');	# Skip the country since we do the states.
      next if ($code eq 'ca');

      if ($geom->{type} eq 'MultiPolygon') {
        $geom = $geom->{coordinates};
      } elsif ($geom->{type} eq 'Polygon') {
        $geom = [ $geom->{coordinates} ];
      } else {
        error ("unknown geom: " . $geom->{type});
      }
  
      # MultiPolygon is a list of polygons.
      # Polygon is a list of closed paths (holes created by winding rule)
      #
      my $i = 0;
      my $j = 0;
      my @polys = ();
      foreach my $poly (@$geom) {
        my @paths = ();
        foreach my $path (@{$poly}) {
          my @points = ();
          foreach my $point (@{$path}) {
            foreach my $n (@$point) {
              # Triangle crashes, related to duplicate points, if we use %.6f.
              # To avoid duplicates entirely, we need to use %.12f.
              # Using %.7f avoids the crash, but valgrind still triggers.
              # It takes %.13f to pass valgrind.
              $n = sprintf ("%.13f", $n);
              $n =~ s/0+$//;
              $n =~ s/\.$//s;
              push @points, $n;
            }
          }

          # The JSON files close the loop explicitly, so remove the last point.
          pop @points;
          pop @points;

          # Also remove any adjascent points that are identical; the JSON
          # contains some points that differ by less than "%.6f".
          {
            my @p2 = ();
            while (@points) {
              my ($x, $y) = (shift @points, shift @points);
              push @p2, ($x, $y)
                unless (@p2 && $p2[@p2-2] == $x && $p2[@p2-1] == $y);
            }
            @points = @p2;

            my %dups;
            while (@p2) {
              my ($x, $y) = (shift @p2, shift @p2);
              my $k = "$x,$y";
              if ($dups{$k}) {
                print STDERR "$progname: WARNING: $name: dup point $k\n";
                # error ("$name: dup point $k") if ($dups{$k});
              }
              $dups{$k} = 1;
            }
          }

          # Write out an array of points, and a struct to hold them.
          my $pn  = $code . 'p' . $j . 'a';
          my $pn2 = $code . 'p' . $j . 'd';
          push @paths, $pn2;
          $out .= ('static const double ' . $pn . '[]={' .
                   join(',', @points) . "};\n" .
                   'static const country_path ' . $pn2 . '={' .
                   scalar(@points) . ',' . $pn . "};\n");
          $j++;
        }
  
        # Write out an array of paths, and a struct to hold them.
        my $pn  = $code . 's' . $i . 'a';
        my $pn2 = $code . 's' . $i . 'd';
        push @polys, $pn2;

        $out .= ('static const country_path ' . $pn . '[]={' .
                 join(',', @paths) . "};\n" .
                 'static const country_polys ' . $pn2 . '={' .
                 scalar(@paths) . ',' . $pn . "};\n");
        $i++;
      }
  
      # Write out an array of polygons, and a struct to hold them.
      my $pn  = $code . 's' . $i . 'a';
      my $pn2 = $code . 'g';
      push @geoms, $pn2;

     #$endonym = '' if (lc($name) eq lc($endonym));
      $endonym = '' if ($name =~ m/\Q$endonym\E/si);  # Contains
      $endonym = '' if ($endonym =~ m/\Q$name\E/si);

     #$endonym2 = '' if (lc($name) eq lc($endonym2));
      $endonym2 = '' if ($name =~ m/\Q$endonym2\E/si);  # Contains
      $endonym2 = '' if ($endonym2 =~ m/\Q$name\E/si);

      $endonym2 = '' if (lc($endonym) eq lc($endonym2));

      $name =~ s/( Is\.).*/$1/s;	# "Føroyar Is. (Faeroe Is.)"
      $name =~ s/ part\)/)/si;		# "Sint Maarten (Dutch part)"

      utf8::encode($name);  # Split wide chars into multi-byte sequences.
      utf8::encode($endonym);
      utf8::encode($endonym2);

      # Emit UTF-8 sequences as octal escapes.
      foreach ($name, $endonym, $endonym2) {
        s@([^ -\177])@{ sprintf("\\%03o", ord($1)); }@gsex;
        s@([^ -\177])@{ sprintf("\\%03o", ord($1)); }@gsex;
      }

      $code = uc($code);
      $code =~ s/_/-/gs;
      $out .= ('static const country_polys ' . $pn . '[]={' .
               join(',', @polys) . "};\n" .
               'static const country_geom ' . $pn2 . '={' .
               "\"$code\",_(\"$name\"),\"$endonym\",\"$endonym2\"," .
               "$pop,$area," .
               scalar(@polys) . ",$pn};\n\n");
    }
  }

  # Write out an array of countries, and a struct to hold them.
  my $pn  = 'countries_geom';
  $out .= ('static const country_geom ' . $pn . "[]={" .
           join(",", @geoms) . "};\n" .
           "const country_info all_countries={" .
           scalar(@geoms) . ',' . $pn . "};\n\n");

  # Saves about 170 KB in a 3.5 MB file.
  $out =~ s/^static const double/D/gm;
  $out =~ s/^static const country_path/C/gm;
  $out =~ s/^static const country_polys/P/gm;
  $out =~ s/^static const country_geom/G/gm;

  open (my $of, '>', $outfile) || error ("$outfile: $!");
  print $of $out;
  close $out;
  print STDERR "$progname: wrote $outfile\n" if ($verbose);
}


sub error($) {
  my ($err) = @_;
  print STDERR "$progname: $err\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname [--verbose] [--quiet] IN.json ... --out OUT.c\n";
  exit 1;
}

sub main() {
  binmode (STDOUT, ':utf8');
  binmode (STDERR, ':utf8');
  my $out;
  my $reuse_p = 0;
  my @files = ();
  while (@ARGV) {
    $_ = shift @ARGV;
    if (m/^--?verbose$/s) { $verbose++; }
    elsif (m/^-v+$/s)     { $verbose += length($_)-1; }
    elsif (m/^--?quiet/s) { $verbose = 0; }
    elsif (m/^--?reuse/s) { $reuse_p = 1; }
    elsif (m/^--?out/s)   { $out = shift @ARGV; }
    elsif (m/^-./s)       { usage; }
    else                  { push @files, $_; }
  }

  usage unless ($out && @files);
  build_countries ($out, $reuse_p, @files);
}

main();
exit 0;
