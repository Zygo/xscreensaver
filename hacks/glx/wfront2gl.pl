#!/usr/bin/perl -w
# Copyright © 2003 Jamie Zawinski <jwz@jwz.org>
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
# Assumes that the OBJ file already contains all vertex normals.
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
my $version = q{ $Revision: 1.2 $ }; $version =~ s/^[^0-9]+([0-9.]+).*$/$1/;

my $verbose = 0;


# convert a vector to a unit vector
sub normalize {
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
sub face_normal {
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


sub parse_obj {
  my ($filename, $obj, $normalize_p) = @_;

  $_ = $obj;
  my @verts = ();
  my @norms = ();
  my @faces = ();

  my @lines = split (/\n/, $obj);

  my $i = -1;
  while (++$i <= $#lines) {
    $_ = $lines[$i];
    next if (m/^\s*$|^\s*\#/);

    if (m/^v\s/) {
      my ($x, $y, $z) = m/^v\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s*$/;
      error ("unpsrable V line: $_") unless defined($z);
      push @verts, ($x+0, $y+0, $z+0);

    } elsif (m/^vn\s/) {
      my ($x, $y, $z) = m/^vn\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s*$/;
      error ("unpsrable VN line: $_") unless defined($z);
      push @norms, ($x+0, $y+0, $z+0);

    } elsif (m/^g\b/) {
      # group name

    } elsif (m/f\s/) {
      my ($a, $b, $c, $d, $e, $f) =
        m/^f\s+
          ([^\s]+)\s+
          ([^\s]+)\s+
          ([^\s]+)\s*
          ([^\s]+)?\s*
          ([^\s]+)?\s*
          ([^\s]+)?\s*
         $/x;
      error ("unpsrable F line: $_") unless defined($c);

      # lose texture coord, if any
      $a =~ s@/.*$@@;
      $b =~ s@/.*$@@;
      $c =~ s@/.*$@@;
      $d =~ s@/.*$@@ if defined($d);
      $e =~ s@/.*$@@ if defined($e);
      $f =~ s@/.*$@@ if defined($f);

      push @faces, ($a-1, $b-1, $c-1);
      push @faces, ($a-1, $c-1, $d-1) if (defined($d));
      push @faces, ($a-1, $d-1, $e-1) if (defined($e));
      push @faces, ($a-1, $e-1, $f-1) if (defined($f));

    } elsif (m/^s\b/) {
      # ???
    } elsif (m/^(mtllib|usemtl)\b/) {
      # ???
    } else {
      error ("unknown line: $_");
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
    foreach my $n (@verts) {
      if    ($i == 0) { $minx = $n if ($n < $minx);
                        $maxx = $n if ($n > $maxx); }
      elsif ($i == 1) { $miny = $n if ($n < $miny);
                        $maxy = $n if ($n > $maxy); }
      else            { $minz = $n if ($n < $minz);
                        $maxz = $n if ($n > $maxz); }
      $i = 0 if (++$i == 3);
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
        $n /= $size;
      }
    }
  }

  # generate interleaved list of triangle coordinates and normals
  #
  my @triangles = ();
  my $nfaces = ($#faces+1)/3;
  for ($i = 0; $i < $nfaces; $i++) {
    my $a = $faces[$i*3];
    my $b = $faces[$i*3+1];
    my $c = $faces[$i*3+2];

    my $x1 = $verts[$a*3];    my $nx1 = $norms[$a*3];
    my $y1 = $verts[$a*3+1];  my $ny1 = $norms[$a*3+1];
    my $z1 = $verts[$a*3+2];  my $nz1 = $norms[$a*3+2];

    my $x2 = $verts[$b*3];    my $nx2 = $norms[$b*3];
    my $y2 = $verts[$b*3+1];  my $ny2 = $norms[$b*3+1];
    my $z2 = $verts[$b*3+2];  my $nz2 = $norms[$b*3+2];

    my $x3 = $verts[$c*3];    my $nx3 = $norms[$c*3];
    my $y3 = $verts[$c*3+1];  my $ny3 = $norms[$c*3+1];
    my $z3 = $verts[$c*3+2];  my $nz3 = $norms[$c*3+2];

    if (!defined($nz3)) {
      my ($nx, $ny, $nz) = face_normal ($x1, $y1, $z1,
                                        $x2, $y2, $z2,
                                        $x3, $y3, $z3);
      $nx1 = $nx2 = $nx3 = $nx;
      $ny1 = $ny2 = $ny3 = $ny;
      $nz1 = $nz2 = $nz3 = $nz;
    }

    push @triangles, ($nx1, $ny1, $nz1,  $x1, $y1, $z1,
                      $nx2, $ny2, $nz2,  $x2, $y2, $z2,
                      $nx3, $ny3, $nz3,  $x3, $y3, $z3);
  }

  return (@triangles);
}


sub generate_c {
  my ($filename, @points) = @_;

  my $code = '';

  $code .= "#include \"gllist.h\"\n";
  $code .= "static const float data[]={\n";

  my $npoints = ($#points + 1) / 6;
  my $nfaces = $npoints / 3;

  for (my $i = 0; $i < $nfaces; $i++) {
    my $nax = $points[$i*18];
    my $nay = $points[$i*18+1];
    my $naz = $points[$i*18+2];

    my  $ax = $points[$i*18+3];
    my  $ay = $points[$i*18+4];
    my  $az = $points[$i*18+5];

    my $nbx = $points[$i*18+6];
    my $nby = $points[$i*18+7];
    my $nbz = $points[$i*18+8];

    my  $bx = $points[$i*18+9];
    my  $by = $points[$i*18+10];
    my  $bz = $points[$i*18+11];

    my $ncx = $points[$i*18+12];
    my $ncy = $points[$i*18+13];
    my $ncz = $points[$i*18+14];

    my  $cx = $points[$i*18+15];
    my  $cy = $points[$i*18+16];
    my  $cz = $points[$i*18+17];

    my $lines = sprintf("\t" . "%.6f,%.6f,%.6f," . "%.6f,%.6f,%.6f,\n" .
                        "\t" . "%.6f,%.6f,%.6f," . "%.6f,%.6f,%.6f,\n" .
                        "\t" . "%.6f,%.6f,%.6f," . "%.6f,%.6f,%.6f,\n",
                        $nax, $nay, $naz,  $ax, $ay, $az,
                        $nbx, $nby, $nbz,  $bx, $by, $bz,
                        $ncx, $ncy, $ncz,  $cx, $cy, $cz);
    $lines =~ s/([.\d])0+,/$1,/g;  # lose trailing insignificant zeroes
    $lines =~ s/\.,/,/g;

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
               (($#points+1)/3) . " points, " .
               (($#points+1)/9) . " faces.\n"
    if ($verbose);

  return $code;
}


sub obj_to_gl {
  my ($infile, $outfile, $normalize_p) = @_;
  local *IN;
  my $obj = '';
  open (IN, "<$infile") || error ("$infile: $!");
  my $filename = ($infile eq '-' ? "<stdin>" : $infile);
  print STDERR "$progname: reading $filename...\n"
    if ($verbose);
  while (<IN>) { $obj .= $_; }
  close IN;

  $obj =~ s/\r\n/\n/g; # CRLF -> LF
  $obj =~ s/\r/\n/g;   # CR -> LF

  my @data = parse_obj ($filename, $obj, $normalize_p);

  $filename = ($outfile eq '-' ? "<stdout>" : $outfile);
  my $code = generate_c ($filename, @data);

  local *OUT;
  open (OUT, ">$outfile") || error ("$outfile: $!");
  print OUT $code || error ("$filename: $!");
  close OUT || error ("$filename: $!");

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
