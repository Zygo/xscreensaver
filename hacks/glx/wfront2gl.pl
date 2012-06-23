#!/usr/bin/perl -w
# Copyright © 2003-2012 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# Reads a Wavefront OBJ file, and emits C data suitable for use with OpenGL's
# glInterleavedArrays() and glDrawArrays() routines.
#
# If the OBJ file does not contain face normals, they are computed.
# Texture coordinates are ignored.
#
# Options:
#
#    --normalize      Compute the bounding box of the object, and scale all
#                     coordinates so that the object fits inside a unit cube.
#
# Created:  8-Mar-2003.

require 5;
use diagnostics;
use strict;

my $progname = $0; $progname =~ s@.*/@@g;
my $version = q{ $Revision: 1.5 $ }; $version =~ s/^[^0-9]+([0-9.]+).*$/$1/;

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


sub parse_obj($$$) {
  my ($filename, $obj, $normalize_p) = @_;

  $_ = $obj;
  my @verts = ();    # list of refs of coords, [x, y, z]
  my @norms = ();    # list of refs of coords, [x, y, z]
  my @texts = ();    # list of refs of coords, [u, v]
  my @faces = ();    # list of refs of [ point, point, point, ... ]
                     #  where 'point' is a ref of indexes into the
                     #  above lists, [ vert, text, norm ]

  my $lineno = 0;
  foreach (split (/\n/, $obj)) {
    $lineno++;
    next if (m/^\s*$|^\s*\#/);

    if (m/^v\s/) {
      my ($x, $y, $z) = m/^v\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s*$/;
      error ("line $lineno: unparsable V line: $_") unless defined($z);
      push @verts, [$x+0, $y+0, $z+0];

    } elsif (m/^vn\s/) {
      my ($x, $y, $z) = m/^vn\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s*$/;
      error ("line $lineno: unparsable VN line: $_") unless defined($z);
      push @norms, [$x+0, $y+0, $z+0];

    } elsif (m/^vt\s/) {
      my ($u, $v) = m/^vt\s+([^\s]+)\s+([^\s]+)\s*$/;
      error ("line $lineno: unparsable VT line: $_") unless defined($v);
      push @texts, [$u+0, $v+0];

    } elsif (m/^g\b/) {
      # group name

    } elsif (m/f\s/) {
      my @f = split(/\s+/, $_);
      shift @f;
      my @vs = ();
      foreach my $f (@f) {
        my ($v, $t, $n);
        if    ($f =~ m@^(\d+)$@s)             { $v = $1; }
        elsif ($f =~ m@^(\d+)/(\d+)$@s)       { $v = $1, $t = $2; }
        elsif ($f =~ m@^(\d+)/(\d+)/(\d+)$@s) { $v = $1, $t = $2; $n = $3; }
        elsif ($f =~ m@^(\d+)///?(\d+)$@s)    { $v = $1; $n = $3; }
        else {
          error ("line $lineno: unparsable F line: $_") unless defined($v);
        }
        $t = -1 unless defined($t);
        $n = -1 unless defined($n);
        push @vs, [$v+0, $t+0, $n+0];
      }
      push @faces, \@vs;

    } elsif (m/^s\b/) {
      # turn on smooth shading
    } elsif (m/^(mtllib|usemtl)\b/) {
      # reference to external materials file (textures, etc.)
    } else {
      error ("line $lineno: unknown line: $_");
    }
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
    foreach my $v (@verts) {
      my ($x, $y, $z) = @$v;
      $minx = $x if ($x < $minx);
      $maxx = $x if ($x > $maxx);
      $miny = $y if ($y < $miny);
      $maxy = $y if ($y > $maxy);
      $minz = $z if ($z < $minz);
      $maxz = $z if ($z > $maxz);
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
      foreach my $n (@verts) {
        my @n = @$n;
        foreach (@n) { $_ /= $size; }
        $n = \@n;
      }
    }
  }

  # generate interleaved list of triangle coordinates and normals
  #
  my @triangles = ();
  my $nfaces = $#faces+1;

  foreach my $f (@faces) {
    # $f is [ [v, t, n], [v, t, n],  ... ]

    my @f = @$f;

#    # (Kludge for the companion cube model)
#    if ($#f > 15) {
#      my $i = 12;
#      @f = (@f[$i-1 .. $#f], @f[0 .. $i]);
#    }

    error ("too few points in face") if ($#f < 2);
    my $p1 = shift @f;

    # If there are more than 3 points, do a triangle fan from the first one:
    # [1 2 3] [1 3 4] [1 4 5] etc.  Doesn't always work with convex shapes.

    while ($#f) {
      my $p2 = shift @f;
      my $p3 = $f[0];

      my $x1 = $verts[$p1->[0]-1]->[0]; my $nx1 = $norms[$p1->[2]-1]->[0];
      my $y1 = $verts[$p1->[0]-1]->[1]; my $ny1 = $norms[$p1->[2]-1]->[1];
      my $z1 = $verts[$p1->[0]-1]->[2]; my $nz1 = $norms[$p1->[2]-1]->[2];

      my $x2 = $verts[$p2->[0]-1]->[0]; my $nx2 = $norms[$p2->[2]-1]->[0];
      my $y2 = $verts[$p2->[0]-1]->[1]; my $ny2 = $norms[$p2->[2]-1]->[1];
      my $z2 = $verts[$p2->[0]-1]->[2]; my $nz2 = $norms[$p2->[2]-1]->[2];

      my $x3 = $verts[$p3->[0]-1]->[0]; my $nx3 = $norms[$p3->[2]-1]->[0];
      my $y3 = $verts[$p3->[0]-1]->[1]; my $ny3 = $norms[$p3->[2]-1]->[1];
      my $z3 = $verts[$p3->[0]-1]->[2]; my $nz3 = $norms[$p3->[2]-1]->[2];

      error ("missing points in face") unless defined($z3);

      if (!defined($nz3)) {
        my ($nx, $ny, $nz) = face_normal ($x1, $y1, $z1,
                                          $x2, $y2, $z2,
                                          $x3, $y3, $z3);
        $nx1 = $nx2 = $nx3 = $nx;
        $ny1 = $ny2 = $ny3 = $ny;
        $nz1 = $nz2 = $nz3 = $nz;
      }


      push @triangles, [$nx1, $ny1, $nz1,  $x1, $y1, $z1,
                        $nx2, $ny2, $nz2,  $x2, $y2, $z2,
                        $nx3, $ny3, $nz3,  $x3, $y3, $z3];
    }
  }

  return (@triangles);
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


sub obj_to_gl($$$) {
  my ($infile, $outfile, $normalize_p) = @_;
  my $obj = '';
  open (my $in, '<', $infile) || error ("$infile: $!");
  my $filename = ($infile eq '-' ? "<stdin>" : $infile);
  print STDERR "$progname: reading $filename...\n"
    if ($verbose);
  while (<$in>) { $obj .= $_; }
  close $in;

  $obj =~ s/\r\n/\n/g; # CRLF -> LF
  $obj =~ s/\r/\n/g;   # CR -> LF

  my @triangles = parse_obj ($filename, $obj, $normalize_p);

  $filename = ($outfile eq '-' ? "<stdout>" : $outfile);
  my $code = generate_c ($filename, @triangles);

  open (my $out, '>', $outfile) || error ("$outfile: $!");
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

  obj_to_gl ($infile, $outfile, $normalize_p);
}

main;
exit 0;
