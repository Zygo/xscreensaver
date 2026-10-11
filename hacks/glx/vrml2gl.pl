#!/usr/bin/perl -w
# Copyright © 2003-2011 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# Reads a VRML WRL file, and emits C data suitable for use with OpenGL's
# glInterleavedArrays() and glDrawArrays() routines.
#
# Face normals are computed.
#
# Options:
#
#    --normalize      Compute the bounding box of the object, and scale all
#                     coordinates so that the object fits inside a unit cube.
#
# Created:  8-Mar-2003 for Wavefront OBJ, converted to VRML 27-Sep-2011.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.2 $' =~ m/\s(\d[.\d]+)\s/s);

my $verbose = 0;


# convert a vector to a unit vector
sub normalize($$$) {
  my ($x, $y, $z) = @_;
  my $L = sqrt (($x * $x) + ($y * $y) + ($z * $z));
  if ($L != 0) {
    $x /= $L;
    $y /= $L;
    $z /= $L;
  } else {
    $x = $y = $z = 0;
  }
  return ($x, $y, $z);
}


# Calculate the unit normal at p0 given two other points p1,p2 on the
# surface.  The normal points in the direction of p1 crossproduct p2.
#
sub face_normal($$$$$$$$$) {
  my ($p0x, $p0y, $p0z,
      $p1x, $p1y, $p1z,
      $p2x, $p2y, $p2z) = @_;

  my ($nx,  $ny,  $nz);
  my ($pax, $pay, $paz);
  my ($pbx, $pby, $pbz);

  $pax = $p1x - $p0x;
  $pay = $p1y - $p0y;
  $paz = $p1z - $p0z;
  $pbx = $p2x - $p0x;
  $pby = $p2y - $p0y;
  $pbz = $p2z - $p0z;
  $nx = $pay * $pbz - $paz * $pby;
  $ny = $paz * $pbx - $pax * $pbz;
  $nz = $pax * $pby - $pay * $pbx;

  return (normalize ($nx, $ny, $nz));
}


sub parse_vrml_1($$$) {
  my ($filename, $body, $normalize_p) = @_;

  my @verts = ();    # list of refs of coords, [x, y, z]
  my @faces = ();    # list of refs of [ point, point, point, ... ]
                     #  where 'point' is a list of indexes into 'verts'.

  $body =~ s% \b point \s* \[ (.*?) \] %{
    foreach my $point (split (/,/, $1)) {
      $point =~ s/^\s+|\s+$//gsi;
      next unless $point;
      my @p = split(/\s+/, $point);
      push @verts, \@p;
    }
  }%gsexi;

  $body =~ s% \b coordIndex \s* \[ (.*?) \] %{
    foreach my $face (split (/\s*,-1,?\s*/, $1)) {
      $face =~ s/^\s+|\s+$//gsi;
      next unless $face;
      my @p = split(/\s*,\s*/, $face);
      push @faces, \@p;
    }
  }%gsexi;

  return () if ($#verts < 0);

  # generate interleaved list of triangle coordinates and normals
  #
  my @triangles = ();
  my $nfaces = $#faces+1;

  foreach my $f (@faces) {
    # $f is [ p1, p2, p3, ... ]

    my @f = @$f;

    error ("too few points in face") if ($#f < 2);
    my $p1 = shift @f;

    # If there are more than 3 points, do a triangle fan from the first one:
    # [1 2 3] [1 3 4] [1 4 5] etc.  Doesn't always work with convex shapes.

    while ($#f) {
      my $p2 = shift @f;
      my $p3 = $f[0];

      my ($pp1, $pp2, $pp3) = ($p1, $p2, $p3);
      # Reverse the winding order.
#      ($pp1, $pp2, $pp3) = ($pp3, $pp2, $pp1);

      my $x1 = $verts[$pp1]->[0];
      my $y1 = $verts[$pp1]->[1];
      my $z1 = $verts[$pp1]->[2];

      my $x2 = $verts[$pp2]->[0];
      my $y2 = $verts[$pp2]->[1];
      my $z2 = $verts[$pp2]->[2];

      my $x3 = $verts[$pp3]->[0];
      my $y3 = $verts[$pp3]->[1];
      my $z3 = $verts[$pp3]->[2];

      error ("missing points in face") unless defined($z3);

      my ($nx, $ny, $nz) = face_normal ($x1, $y1, $z1,
                                        $x2, $y2, $z2,
                                        $x3, $y3, $z3);

      push @triangles, [$nx, $ny, $nz,  $x1, $y1, $z1,
                        $nx, $ny, $nz,  $x2, $y2, $z2,
                        $nx, $ny, $nz,  $x3, $y3, $z3];
    }
  }

  return (@triangles);
}


sub parse_vrml($$$) {
  my ($filename, $body, $normalize_p) = @_;

  my @triangles = ();

  $body =~ s/\s*\#.*$//gmi;  # comments

  # Lose 2D imagery
  $body =~ s/\bIndexedLineSet \s* { \s* coordIndex \s* \[ .*? \] \s* }//gsix;

  $body =~ s/(\bSeparator\b)/\001$1/g;

  foreach my $sec (split (m/\001/, $body)) {
    push @triangles, parse_vrml_1 ($filename, $sec, $normalize_p);
  }


  # find bounding box, and normalize
  #
  if ($normalize_p || $verbose) {
    my $minx =  999999999;
    my $miny =  999999999;
    my $minz =  999999999;
    my $maxx = -999999999;
    my $maxy = -999999999;
    my $maxz = -999999999;
    my $i = 0;

    foreach my $t (@triangles) {
      my ($nx1, $ny1, $nz1,  $x1, $y1, $z1,
          $nx2, $ny2, $nz2,  $x2, $y2, $z2,
          $nx3, $ny3, $nz3,  $x3, $y3, $z3) = @$t;

      foreach my $x ($x1, $x2, $x3) { 
        $minx = $x if ($x < $minx); 
        $maxx = $x if ($x > $maxx);
      }
      foreach my $y ($y1, $y2, $y3) {
        $miny = $y if ($y < $miny);
        $maxy = $y if ($y > $maxy);
      }
      foreach my $z ($z1, $z2, $z3) {
        $minz = $z if ($z < $minz);
        $maxz = $z if ($z > $maxz);
      }
    }

    my $w = ($maxx - $minx);
    my $h = ($maxy - $miny);
    my $d = ($maxz - $minz);
    my $sizea = ($w > $h ? $w : $h);
    my $sizeb = ($w > $d ? $w : $d);
    my $size = ($sizea > $sizeb ? $sizea : $sizeb);
        
    print STDERR "$progname: bbox is " .
                  sprintf("%.2f x %.2f x %.2f\n", $w, $h, $d)
       if ($verbose);

    if ($normalize_p) {
      $w /= $size;
      $h /= $size;
      $d /= $size;
      print STDERR "$progname: dividing by $size for bbox of " .
                  sprintf("%.2f x %.2f x %.2f\n", $w, $h, $d)
        if ($verbose);

      foreach my $t (@triangles) {
        my @t = @$t;
        $t[3]  /= $size; $t[4]  /= $size; $t[5]  /= $size;
        $t[9]  /= $size; $t[10] /= $size; $t[11] /= $size;
        $t[15] /= $size; $t[16] /= $size; $t[17] /= $size;
        $t = \@t;
      }
    }
  }

  return @triangles;
}


sub generate_c($@) {
  my ($filename, @triangles) = @_;

  my $code = '';

  $code .= "#include \"gllist.h\"\n";
  $code .= "static const float data[]={\n";

  my $nfaces = $#triangles + 1;
  my $npoints = $nfaces * 3;

  foreach my $t (@triangles) {
    my ($nx1, $ny1, $nz1,  $x1, $y1, $z1,
        $nx2, $ny2, $nz2,  $x2, $y2, $z2,
        $nx3, $ny3, $nz3,  $x3, $y3, $z3) = @$t;
    my $lines = sprintf("\t" . "%.6f,%.6f,%.6f," . "%.6f,%.6f,%.6f,\n" .
                        "\t" . "%.6f,%.6f,%.6f," . "%.6f,%.6f,%.6f,\n" .
                        "\t" . "%.6f,%.6f,%.6f," . "%.6f,%.6f,%.6f,\n",
                        $nx1, $ny1, $nz1,  $x1, $y1, $z1,
                        $nx2, $ny2, $nz2,  $x2, $y2, $z2,
                        $nx3, $ny3, $nz3,  $x3, $y3, $z3);
    $lines =~ s/([.\d])0+,/$1,/g;  # lose trailing insignificant zeroes
    $lines =~ s/\.,/,/g;
    $lines =~ s/-0,/0,/g;

    $code .= $lines;
  }

  my $token = $filename;    # guess at a C token from the filename
  $token =~ s/\<[^<>]*\>//;
  $token =~ s@^.*/@@;
  $token =~ s/\.[^.]*$//;
  $token =~ s/[^a-z\d]/_/gi;
  $token =~ s/__+/_/g;
  $token =~ s/^_//g;
  $token =~ s/_$//g;
  $token =~ tr [A-Z] [a-z];
  $token = 'foo' if ($token eq '');

  my $format = 'GL_N3F_V3F';
  my $primitive = 'GL_TRIANGLES';

  $code =~ s/,\n$//s;
  $code .= "\n};\n";
  $code .= "static const struct gllist frame={";
  $code .= "$format,$primitive,$npoints,data,NULL};\n";
  $code .= "const struct gllist *$token=&frame;\n";

  print STDERR "$filename: " .
               (($#triangles+1)*3) . " points, " .
               (($#triangles+1))   . " faces.\n"
    if ($verbose);

  return $code;
}


sub vrml_to_gl($$$) {
  my ($infile, $outfile, $normalize_p) = @_;
  my $body = '';

  my $in;
  if ($infile eq '-') {
    $in = *STDIN;
  } else {
    open ($in, '<', $infile) || error ("$infile: $!");
  }
  my $filename = ($infile eq '-' ? "<stdin>" : $infile);
  print STDERR "$progname: reading $filename...\n"
    if ($verbose);
  while (<$in>) { $body .= $_; }
  close $in;

  $body =~ s/\r\n/\n/g; # CRLF -> LF
  $body =~ s/\r/\n/g;   # CR -> LF

  my @triangles = parse_vrml ($filename, $body, $normalize_p);

  $filename = ($outfile eq '-' ? "<stdout>" : $outfile);
  my $code = generate_c ($filename, @triangles);

  my $out;
  if ($outfile eq '-') {
    $out = *STDOUT;
  } else {
    open ($out, '>', $outfile) || error ("$outfile: $!");
  }
  (print $out $code) || error ("$filename: $!");
  (close $out) || error ("$filename: $!");

  print STDERR "$progname: wrote $filename\n"
    if ($verbose || $outfile ne '-');
}


sub error {
  ($_) = @_;
  print STDERR "$progname: $_\n";
  exit 1;
}

sub usage {
  print STDERR "usage: $progname [--verbose] [infile [outfile]]\n";
  exit 1;
}

sub main {
  my ($infile, $outfile);
  my $normalize_p = 0;
  while ($_ = $ARGV[0]) {
    shift @ARGV;
    if ($_ eq "--verbose") { $verbose++; }
    elsif (m/^-v+$/) { $verbose += length($_)-1; }
    elsif ($_ eq "--normalize") { $normalize_p = 1; }
    elsif (m/^-./) { usage; }
    elsif (!defined($infile)) { $infile = $_; }
    elsif (!defined($outfile)) { $outfile = $_; }
    else { usage; }
  }

  $infile  = "-" unless defined ($infile);
  $outfile = "-" unless defined ($outfile);

  vrml_to_gl ($infile, $outfile, $normalize_p);
}

main;
exit 0;
