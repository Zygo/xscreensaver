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
# Reads a DXF file, and emits C data suitable for use with OpenGL's
# glInterleavedArrays() and glDrawArrays() routines.
#
# Options:
#
#    --normalize      Compute the bounding box of the object, and scale all
#                     coordinates so that the object fits inside a unit cube.
#
#    --smooth         When computing normals for the vertexes, average the
#                     normals at any edge which is less than 90 degrees.
#                     If this option is not specified, planar normals will be
#                     used, resulting in a "faceted" object.
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


# why this isn't in perlfunc, I don't know.
sub acos { atan2( sqrt(1 - $_[0] * $_[0]), $_[0] ) }
my $pi = 3.141592653589793;
my $radians_to_degrees = 180.0 / $pi;

# Calculate the angle (in degrees) between two vectors.
#
sub vector_angle {
  my ($x1, $y1, $z1,
      $x2, $y2, $z2) = @_;

  my $L1 = sqrt ($x1*$x1 + $y1*$y1 + $z1*$z1);
  my $L2 = sqrt ($x2*$x2 + $y2*$y2 + $z2*$z2);

  return 0 if ($L1 == 0 || $L2 == 0);
  return 0 if ($x1 == $x2 && $y1 == $y2 && $z1 == $z2);

  # dot product of two vectors is defined as:
  #   $L1 * $L1 * cos(angle between vectors)
  # and is also defined as:
  #   $x1*$x2 + $y1*$y2 + $z1*$z2
  # so:
  #   $L1 * $L1 * cos($angle) = $x1*$x2 + $y1*$y2 + $z1*$z2
  #   cos($angle) = ($x1*$x2 + $y1*$y2 + $z1*$z2) / ($L1 * $L2)
  #   $angle = acos (($x1*$x2 + $y1*$y2 + $z1*$z2) / ($L1 * $L2));
  #
  my $cos = ($x1*$x2 + $y1*$y2 + $z1*$z2) / ($L1 * $L2);
  $cos = 1 if ($cos > 1);  # avoid fp rounding error (1.000001 => sqrt error)
  my $angle = acos ($cos);

  return ($angle * $radians_to_degrees);
}

# given a list of triangles ( [ X1, Y1, Z1,  X2, Y2, Z2,  X3, Y3, Z3, ]+ )
# returns a list of the normals for each vertex.
#
sub compute_vertex_normals {
  my (@points) = @_;
  my $npoints = ($#points+1) / 3;
  my $nfaces = $npoints / 3;

  my @face_normals = ();
  my %point_faces;

  for (my $i = 0; $i < $nfaces; $i++) {
    my ($ax, $ay, $az,  $bx, $by, $bz,  $cx, $cy, $cz) =
      @points[($i*9) .. ($i*9)+8];

    # store the normal for each face in the $face_normals array
    # indexed by face number.
    #
    my @norm = face_normal ($ax, $ay, $az,
                            $bx, $by, $bz,
                            $cx, $cy, $cz);
    $face_normals[$i] = \@norm;

    # store in the %point_faces hash table a list of every face number
    # in which a point participates

    my $p;
    my @flist;

    $p = "$ax $ay $az";
    @flist = (defined($point_faces{$p}) ? @{$point_faces{$p}} : ());
    push @flist, $i;
    $point_faces{$p} = \@flist;

    $p = "$bx $by $bz";
    @flist = (defined($point_faces{$p}) ? @{$point_faces{$p}} : ());
    push @flist, $i;
    $point_faces{$p} = \@flist;

    $p = "$cx $cy $cz";
    @flist = (defined($point_faces{$p}) ? @{$point_faces{$p}} : ());
    push @flist, $i;
    $point_faces{$p} = \@flist;
  }


  # compute the normal for each vertex of each face.
  # (these points are not unique -- because there might be multiple
  # normals associated with the same vertex for different faces,
  # in the case where it's a sharp angle.)
  #
  my @normals = ();
  for (my $i = 0; $i < $nfaces; $i++) {
    my @verts = @points[($i*9) .. ($i*9)+8];
    error ("overshot in points?") unless defined($verts[8]);

    my @norm = @{$face_normals[$i]};
    error ("no normal $i?") unless defined($norm[2]);

    # iterate over the (three) vertexes in this face.
    #
    for (my $j = 0; $j < 3; $j++) {
      my ($x, $y, $z) = @verts[($j*3) .. ($j*3)+2];
      error ("overshot in verts?") unless defined($z);

      # Iterate over the faces in which this point participates.
      # This face's normal is the average of the normals of those faces.
      # Except, faces are ignored if any point in them is at more than
      # a 90 degree angle from the zeroth face (arbitrarily.)
      #
      my ($nx, $ny, $nz) = (0, 0, 0);
      my @faces = @{$point_faces{"$x $y $z"}};
      foreach my $fn (@faces) {
        my ($ax, $ay, $az,  $bx, $by, $bz,  $cx, $cy, $cz) =
          @points[($fn*9) .. ($fn*9)+8];
        my @fnorm = @{$face_normals[$fn]};

        # ignore any adjascent faces that are more than 90 degrees off.
        my $angle = vector_angle (@norm, @fnorm);
        next if ($angle >= 90);

        $nx += $fnorm[0];
        $ny += $fnorm[1];
        $nz += $fnorm[2];
      }

      push @normals, normalize ($nx, $ny, $nz);
    }
  }

  return @normals;
}



sub parse_dxf {
  my ($filename, $dxf, $normalize_p) = @_;

  $_ = $dxf;
  my ($points_txt, $coords_txt);
  my $vvers;

  $dxf =~ s/([^\n]*)\n([^\n]*)\n/$1\t$2\n/g;  # join even and odd lines

  my @triangles = ();

  my @items = split (/\n/, $dxf);
  while ($#items >= 0) {

    $_ = shift @items;

    if      (m/^\s* 0 \s+ SECTION \b/x) {
    } elsif (m/^\s* 2 \s+ HEADER \b/x) {
    } elsif (m/^\s* 0 \s+ ENDSEC \b/x) {
    } elsif (m/^\s* 2 \s+ ENTITIES \b/x) {
    } elsif (m/^\s* 0 \s+ EOF \b/x) {
    } elsif (m/^\s* 0 \s+ 3DFACE \b/x) {

      my @points = ();
      my $pc = 0;

      while ($#items >= 0) {
        $_ = shift @items;  # get next line

        my $d = '(-?\d+\.?\d+)';
        if (m/^\s* 8 \b/x) {        # layer name
        } elsif (m/^\s* 62 \b/x) {  # color number

        } elsif (m/^\s* 10 \s+ $d/xo) { $pc++; $points[ 0] = $1;    # X1
        } elsif (m/^\s* 20 \s+ $d/xo) { $pc++; $points[ 1] = $1;    # Y1
        } elsif (m/^\s* 30 \s+ $d/xo) { $pc++; $points[ 2] = $1;    # Z1

        } elsif (m/^\s* 11 \s+ $d/xo) { $pc++; $points[ 3] = $1;    # X2
        } elsif (m/^\s* 21 \s+ $d/xo) { $pc++; $points[ 4] = $1;    # Y2
        } elsif (m/^\s* 31 \s+ $d/xo) { $pc++; $points[ 5] = $1;    # Z2

        } elsif (m/^\s* 12 \s+ $d/xo) { $pc++; $points[ 6] = $1;    # X3
        } elsif (m/^\s* 22 \s+ $d/xo) { $pc++; $points[ 7] = $1;    # Y3
        } elsif (m/^\s* 32 \s+ $d/xo) { $pc++; $points[ 8] = $1;    # Z3

        } elsif (m/^\s* 13 \s+ $d/xo) { $pc++; $points[ 9] = $1;    # X4
        } elsif (m/^\s* 23 \s+ $d/xo) { $pc++; $points[10] = $1;    # Y4
        } elsif (m/^\s* 33 \s+ $d/xo) { $pc++; $points[11] = $1;    # Z4
        } else {
          error ("$filename: unknown 3DFACE entry: $_\n");
        }

        last if ($pc >= 12);
      }

      if ($points[6] != $points[9] ||
          $points[7] != $points[10] ||
          $points[8] != $points[11]) {
        error ("$filename: got a quad, not a triangle\n");
      } else {
        @points = @points[0 .. 8];
      }

      foreach (@points) { $_ += 0; }    # convert strings to numbers

      push @triangles, @points;

    } else {
      error ("$filename: unknown: $_\n");
    }
  }


  my $npoints = ($#triangles+1) / 3;


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
    foreach my $n (@triangles) {
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
        
    print STDERR "$progname: $filename: bbox is " .
                  sprintf("%.2f x %.2f x %.2f\n", $w, $h, $d)
       if ($verbose);

    if ($normalize_p) {
      $w /= $size;
      $h /= $size;
      $d /= $size;
      print STDERR "$progname: $filename: dividing by $size for bbox of " .
                  sprintf("%.2f x %.2f x %.2f\n", $w, $h, $d)
        if ($verbose);
      foreach my $n (@triangles) {
        $n /= $size;
      }
    }
  }

  return (@triangles);
}


sub generate_c {
  my ($filename, $smooth_p, @points) = @_;

  my $ccw_p = 1;  # counter-clockwise winding rule for computing normals

  my $code = '';

  $code .= "#include \"gllist.h\"\n";
  $code .= "static const float data[]={\n";

  my $npoints = ($#points + 1) / 3;
  my $nfaces = $npoints / 3;

  my @normals;
  if ($smooth_p) {
    @normals = compute_vertex_normals (@points);

    if ($#normals != $#points) {
      error ("computed " . (($#normals+1)/3) . " normals for " .
             (($#points+1)/3) . " points?");
    }
  }

  for (my $i = 0; $i < $nfaces; $i++) {
    my $ax = $points[$i*9];
    my $ay = $points[$i*9+1];
    my $az = $points[$i*9+2];

    my $bx = $points[$i*9+3];
    my $by = $points[$i*9+4];
    my $bz = $points[$i*9+5];

    my $cx = $points[$i*9+6];
    my $cy = $points[$i*9+7];
    my $cz = $points[$i*9+8];

    my ($nax, $nay, $naz,
        $nbx, $nby, $nbz,
        $ncx, $ncy, $ncz);

    if ($smooth_p) {
      $nax = $normals[$i*9];
      $nay = $normals[$i*9+1];
      $naz = $normals[$i*9+2];

      $nbx = $normals[$i*9+3];
      $nby = $normals[$i*9+4];
      $nbz = $normals[$i*9+5];

      $ncx = $normals[$i*9+6];
      $ncy = $normals[$i*9+7];
      $ncz = $normals[$i*9+8];

    } else {
      if ($ccw_p) {
        ($nax, $nay, $naz) = face_normal ($ax, $ay, $az,
                                          $bx, $by, $bz,
                                          $cx, $cy, $cz);
      } else {
        ($nax, $nay, $naz) = face_normal ($ax, $ay, $az,
                                          $cx, $cy, $cz,
                                          $bx, $by, $bz);
      }
      ($nbx, $nby, $nbz) = ($nax, $nay, $naz);
      ($ncx, $ncy, $ncz) = ($nax, $nay, $naz);
    }

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

  print STDERR "$progname: $filename: " .
               (($#points+1)/3) . " points, " .
               (($#points+1)/9) . " faces.\n"
    if ($verbose);

  return $code;
}


sub dxf_to_gl {
  my ($infile, $outfile, $smooth_p, $normalize_p) = @_;
  local *IN;
  my $dxf = '';
  open (IN, "<$infile") || error ("$infile: $!");
  my $filename = ($infile eq '-' ? "<stdin>" : $infile);
  print STDERR "$progname: reading $filename...\n"
    if ($verbose);
  while (<IN>) { $dxf .= $_; }
  close IN;

  $dxf =~ s/\r\n/\n/g; # CRLF -> LF
  $dxf =~ s/\r/\n/g;   # CR -> LF

  my @data = parse_dxf ($filename, $dxf, $normalize_p);

  $filename = ($outfile eq '-' ? "<stdout>" : $outfile);
  my $code = generate_c ($filename, $smooth_p, @data);

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
  print STDERR "usage: $progname [--verbose] [--smooth] [infile [outfile]]\n";
  exit 1;
}

sub main {
  my ($infile, $outfile);
  my $normalize_p = 0;
  my $smooth_p = 0;
  while ($_ = $ARGV[0]) {
    shift @ARGV;
    if ($_ eq "--verbose") { $verbose++; }
    elsif (m/^-v+$/) { $verbose += length($_)-1; }
    elsif ($_ eq "--normalize") { $normalize_p = 1; }
    elsif ($_ eq "--smooth") { $smooth_p = 1; }
    elsif (m/^-./) { usage; }
    elsif (!defined($infile)) { $infile = $_; }
    elsif (!defined($outfile)) { $outfile = $_; }
    else { usage; }
  }

  $infile  = "-" unless defined ($infile);
  $outfile = "-" unless defined ($outfile);

  dxf_to_gl ($infile, $outfile, $smooth_p, $normalize_p);
}

main;
exit 0;
