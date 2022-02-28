#!/usr/bin/perl -w
# Copyright © 2003-2022 Jamie Zawinski <jwz@jwz.org>
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
#    --smooth [DEG]   When computing normals for the vertexes, average the
#                     normals at any edge which is less than N degrees.
#                     If this option is not specified, planar normals will be
#                     used, resulting in a "faceted" object.
#
#    --wireframe      Emit lines instead of faces.
#
#    --layers         Emit a separate set of polygons for each layer in the
#                     input file, instead of emitting the whole file as a
#                     single unit.
#
# Created:  8-Mar-2003.

require 5;
use diagnostics;
use strict;

use POSIX qw(mktime strftime);
use Math::Trig qw(acos);
use Text::Wrap;

my $progname = $0; $progname =~ s@.*/@@g;
my ($version) = ('$Revision: 1.14 $' =~ m/\s(\d[.\d]+)\s/s);

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


my $pi = 3.141592653589793;
my $radians_to_degrees = 180.0 / $pi;

# Calculate the angle (in degrees) between two vectors.
#
sub vector_angle($$$$$$) {
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
# returns a list of the normals for each vertex.  These are the smoothed
# normals: the average of the normals of the participating faces.
#
sub compute_vertex_normals($@) {
  my ($smooth, @points) = @_;
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

    foreach my $p ("$ax $ay $az", "$bx $by $bz", "$cx $cy $cz") {
      my @flist = (defined($point_faces{$p}) ? @{$point_faces{$p}} : ());
      push @flist, $i;
      $point_faces{$p} = \@flist;
    }
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
      # But ignore any other faces that are at more than an N degree
      # angle from this point's face. Those are sharp edges.
      #
      my ($nx, $ny, $nz) = (0, 0, 0);
      my @faces = @{$point_faces{"$x $y $z"}};
      foreach my $fn (@faces) {
        my ($ax, $ay, $az,  $bx, $by, $bz,  $cx, $cy, $cz) =
          @points[($fn*9) .. ($fn*9)+8];
        my @fnorm = @{$face_normals[$fn]};

        # ignore any adjascent faces that are more than N degrees off.
        my $angle = vector_angle ($norm[0],  $norm[1],  $norm[2],
                                  $fnorm[0], $fnorm[1], $fnorm[2]);
        next if ($angle >= $smooth);

        $nx += $fnorm[0];
        $ny += $fnorm[1];
        $nz += $fnorm[2];
      }

      push @normals, normalize ($nx, $ny, $nz);
    }
  }

  return @normals;
}


sub parse_dxf($$$$$) {
  my ($filename, $dxf, $normalize_p, $wireframe_p, $layers_p) = @_;

  $dxf =~ s/\r\n/\n/gs;				# CRLF
  $dxf =~ s/^[ \t\n]+|[ \t\n]+$//s;		# leading/trailing whitespace

  # Convert whitespace within a line to _, e.g., "ObjectDBX Classes".
  # What the hell is up with this file format!
  1 while ($dxf =~ s/([^ \t\n])[ \t\#]+([^ \t\n])/$1_$2/gs);

  $dxf =~ s/\r/\n/gs;

  # Turn blank lines into "", e.g., "$DIMBLK \n 1 \n \n 9 \n"
  $dxf =~ s/\n\n/\n""\n/gs;

  my @tokens = split (/[ \t\n]+/, $dxf);	# tokenize

  my @entities = ();				# parse
  while (@tokens) {
    my @elts = ();
    my $key = shift @tokens;			# sectionize at "0 WORD"
    do {
      my $val = shift @tokens;
      push @elts, [ $key, $val ];		# contents are [CODE VAL]
      $key = shift @tokens;
    } while ($key && $key ne 0);
    unshift @tokens, $key if defined($key);
    push @entities, \@elts;
  }
  my %triangles;   # list of points, indexed by layer name
  my %lines;
  my $error_count = 0;

  foreach my $entity (@entities) {
    my $header = shift @$entity;
    my ($code, $name) = @$header;

    if ($name eq 'SECTION' ||
        $name eq 'HEADER' ||
        $name eq 'ENDSEC' ||
        $name eq 'EOF') {
      print STDERR "$progname: $filename: ignoring \"$code $name\"\n"
        if ($verbose > 1);

    } elsif ($name eq '3DFACE') {

      my @points = ();
      my $pc = 0;
      my $layer = '';

      foreach my $entry (@$entity) {
        my ($key, $val) = @$entry;
        if      ($key eq 8)  { $layer = $val;			# layer name

        } elsif ($key eq 10) { $pc++; $points[0] = $val;	# X1
        } elsif ($key eq 20) { $pc++; $points[1] = $val;	# Y1
        } elsif ($key eq 30) { $pc++; $points[2] = $val;	# Z1

        } elsif ($key eq 11) { $pc++; $points[3] = $val;	# X2
        } elsif ($key eq 21) { $pc++; $points[4] = $val;	# Y2
        } elsif ($key eq 31) { $pc++; $points[5] = $val;	# Z2

        } elsif ($key eq 12) { $pc++; $points[6] = $val;	# X3
        } elsif ($key eq 22) { $pc++; $points[7] = $val;	# Y3
        } elsif ($key eq 32) { $pc++; $points[8] = $val;	# Z3

        } elsif ($key eq 13) { $pc++; $points[9]  = $val;	# X4
        } elsif ($key eq 23) { $pc++; $points[10] = $val;	# Y4
        } elsif ($key eq 33) { $pc++; $points[11] = $val;	# Z4

        } elsif ($key eq 62) {				# color number
        } elsif ($key eq 70) {				# invisible edge flag
        } else {
          print STDERR "$progname: $filename: WARNING:" .
            " unknown $name: \"$key $val\"\n";
          $error_count++;
        }
      }

      error ("got $pc points in $name") unless ($pc == 12);

      if ($points[6] != $points[9] ||
          $points[7] != $points[10] ||
          $points[8] != $points[11]) {
        error ("$filename: got a quad, not a triangle\n");
      } else {
        @points = @points[0 .. 8];
      }

      foreach (@points) { $_ += 0; }    # convert strings to numbers

      $layer = '' unless $layers_p;

      $triangles{$layer} = [] unless defined ($triangles{$layer});
      push @{$triangles{$layer}}, @points;

    } elsif ($name eq 'LINE') {

      my @points = ();
      my $pc = 0;
      my $layer = '';

      foreach my $entry (@$entity) {
        my ($key, $val) = @$entry;
        if      ($key eq 8)  { $layer = $val;			# layer name

        } elsif ($key eq 10) { $pc++; $points[0] = $val;	# X1
        } elsif ($key eq 20) { $pc++; $points[1] = $val;	# Y1
        } elsif ($key eq 30) { $pc++; $points[2] = $val;	# Z1

        } elsif ($key eq 11) { $pc++; $points[3] = $val;	# X2
        } elsif ($key eq 21) { $pc++; $points[4] = $val;	# Y2
        } elsif ($key eq 31) { $pc++; $points[5] = $val;	# Z2

        } elsif ($key eq 39) {				# thickness
        } elsif ($key eq 62) {				# color number
        } else {
          print STDERR "$progname: $filename: WARNING:" .
            " unknown $name: \"$key $val\"\n";
          $error_count++;
        }
      }

      error ("got $pc points in $name") unless ($pc == 6);

      foreach (@points) { $_ += 0; }    # convert strings to numbers

      $layer = '' unless $layers_p;

      $lines{$layer} = [] unless defined ($lines{$layer});
      push @{$lines{$layer}}, @points;

    } elsif ($name =~ m/^\d+$/s) {
      error ("sequence lost: \"$code $name\"");

    } else {
      print STDERR "$progname: $filename: WARNING: unknown: \"$code $name\"\n";
      $error_count++;
    }

    error ("too many errors: bailing!") if ($error_count > 50);
  }

  if ($wireframe_p) {

    # Convert faces to lines.
    # Don't duplicate shared edges.

    foreach my $layer (keys %triangles) {
      my %dups;
      my @triangles = @{$triangles{$layer}};
      while (@triangles) {
        my $x1 = shift @triangles; # 0
        my $y1 = shift @triangles; # 1
        my $z1 = shift @triangles; # 2
        my $x2 = shift @triangles; # 3
        my $y2 = shift @triangles; # 4
        my $z2 = shift @triangles; # 5
        my $x3 = shift @triangles; # 6
        my $y3 = shift @triangles; # 7
        my $z3 = shift @triangles; # 8

        my $p = sub(@) {
          my ($x1, $y1, $z1, $x2, $y2, $z2) = @_;
          my $key1 = "$x1, $y1, $z1, $x2, $y2, $z2";
          my $key2 = "$x2, $y2, $z2, $x1, $y1, $z1";
          my $dup = $dups{$key1} || $dups{$key2};
          $dups{$key1} = 1;
          $dups{$key2} = 1;
          push @{$lines{$layer}}, @_ unless $dup;
        }
        ;
        $p->($x1, $y1, $z1, $x2, $y2, $z2);
        $p->($x2, $y2, $z2, $x3, $y3, $z3);
        $p->($x3, $y3, $z3, $x1, $y1, $z1);
      }

      @{$triangles{$layer}} = ();
    }

  } else {
    foreach my $layer (keys %lines) {
      my $n = @{$lines{$layer}};
      @{$lines{$layer}} = ();
      print STDERR "$progname: $filename: $layer: WARNING:" .
                   " ignored $n stray LINE" . ($n == 1 ? "" : "s") . ".\n"
       if ($n);
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

    foreach my $layer ($wireframe_p
                       ? keys %lines
                       : keys %triangles) {
      my %dups;
      my @triangles = ($wireframe_p
                       ? @{$lines{$layer}}
                       : @{$triangles{$layer}});
      foreach my $n (@{$lines{$layer}}, @{$triangles{$layer}}) {
        if    ($i == 0) { $minx = $n if ($n < $minx);
                          $maxx = $n if ($n > $maxx); }
        elsif ($i == 1) { $miny = $n if ($n < $miny);
                          $maxy = $n if ($n > $maxy); }
        else            { $minz = $n if ($n < $minz);
                          $maxz = $n if ($n > $maxz); }
        $i = 0 if (++$i == 3);
      }
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
    print STDERR "$progname: $filename: center is " .
                  sprintf("%.2f, %.2f, %.2f\n",
                          $minx + $w / 2,
                          $miny + $h / 2,
                          $minz + $d / 2)
       if ($verbose);

    if ($normalize_p) {
      $w /= $size;
      $h /= $size;
      $d /= $size;

      print STDERR "$progname: $filename: dividing by " .
                   sprintf("%.2f", $size) . " for bbox of " .
                   sprintf("%.2f x %.2f x %.2f\n", $w, $h, $d)
        if ($verbose);
      foreach my $layer (keys %triangles) {
        foreach my $n (@{$triangles{$layer}}) { $n /= $size; }
        foreach my $n (@{$lines{$layer}})     { $n /= $size; }
      }
    }
  }

  return ($wireframe_p ? \%lines : \%triangles);
}


sub generate_c_1($$$$$@) {
  my ($name, $outfile, $smooth, $wireframe_p, $normalize_p, @points) = @_;

  my $ccw_p = 1;  # counter-clockwise winding rule for computing normals

  my $npoints = ($#points + 1) / 3;
  my $nfaces = ($wireframe_p ? $npoints/2 : $npoints/3);

  my @normals;
  if ($smooth && !$wireframe_p) {
    @normals = compute_vertex_normals ($smooth, @points);

    if ($#normals != $#points) {
      error ("computed " . (($#normals+1)/3) . " normals for " .
             (($#points+1)/3) . " points?");
    }
  }

  my $code .= "\nstatic const float ${name}_data[] = {\n";

  if ($wireframe_p) {
    my %dups;
    for (my $i = 0; $i < $nfaces; $i++) {
      my $ax = $points[$i*6];
      my $ay = $points[$i*6+1];
      my $az = $points[$i*6+2];

      my $bx = $points[$i*6+3];
      my $by = $points[$i*6+4];
      my $bz = $points[$i*6+5];

      my $lines = sprintf("\t" . "%.6f,%.6f,%.6f,\n" .
                          "\t" . "%.6f,%.6f,%.6f,\n",
                          $ax, $ay, $az,
                          $bx, $by, $bz);
      $lines =~ s/([.\d])0+,/$1,/g;  # lose trailing insignificant zeroes
      $lines =~ s/\.,/,/g;
      $lines =~ s/-0,/0,/g;

      $code .= $lines;
    }

  } else {
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

      if ($smooth) {
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
      $lines =~ s/-0,/0,/g;

      $code .= $lines;
    }
  }

  my $format    = ($wireframe_p ? 'GL_V3F'   : 'GL_N3F_V3F');
  my $primitive = ($wireframe_p ? 'GL_LINES' : 'GL_TRIANGLES');

  $code =~ s/,\n$//s;
  $code .= "\n};\n";
  $code .= "static const struct gllist ${name}_frame = {\n";
  $code .= " $format, $primitive, $npoints, ${name}_data, 0\n};\n";
  $code .= "const struct gllist *$name = &${name}_frame;\n";

  print STDERR "$progname: $outfile: $name: $npoints points, $nfaces faces.\n"
    if ($verbose);

  return ($code, $npoints, $nfaces);
}


sub generate_c($$$$$$) {
  my ($infile, $outfile, $smooth, $wireframe_p, $normalize_p, $layers) = @_;

  my $code = '';

  my $token = $outfile;    # guess at a C token from the filename
  $token =~ s/\<[^<>]*\>//;
  $token =~ s@^.*/@@;
  $token =~ s/\.[^.]*$//;
  $token =~ s/[^a-z\d]/_/gi;
  $token =~ s/__+/_/g;
  $token =~ s/^_//g;
  $token =~ s/_$//g;
  $token =~ tr [A-Z] [a-z];
  $token = 'foo' if ($token eq '');

  my @layers = sort (keys %$layers);

  $infile =~ s@^.*/@@s;
  $code .= ("/* Generated from \"$infile\" on " .
            strftime ("%d-%b-%Y", localtime ()) . ".\n" .
            "   " . ($wireframe_p
                     ? "Wireframe."
                     : ($smooth ? 
                        "Smoothed vertex normals at $smooth\x{00B0}." :
                        "Faceted face normals.")) .
            ($normalize_p ? " Normalized to unit bounding box." : "") .
            "\n" .
            (@layers > 1
             ? wrap ("   ", "     ", "Components: " . join (", ", @layers)) . ".\n"
             : "") .
            " */\n\n");

  $code .= "#include \"gllist.h\"\n";

  my $npoints = 0;
  my $nfaces = 0;

  foreach my $layer (@layers) {
    my $name = $layer ? "${token}_${layer}" : $token;
    my ($c, $np, $nf) =
      generate_c_1 ($name, $outfile,
                    $smooth, $wireframe_p, $normalize_p,
                    @{$layers->{$layer}});
    $code .= $c;
    $npoints += $np;
    $nfaces  += $nf;
  }

  print STDERR "$progname: $outfile: total: $npoints points, $nfaces faces.\n"
    if ($verbose && @layers > 1);

  return $code;
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


sub dxf_to_gl($$$$$$) {
  my ($infile, $outfile, $smooth, $normalize_p, $wireframe_p, $layers_p) = @_;

  open (my $in, "<$infile") || error ("$infile: $!");
  my $filename = ($infile eq '-' ? "<stdin>" : $infile);
  print STDERR "$progname: reading $filename...\n"
    if ($verbose);

  local $/ = undef;  # read entire file
  my $dxf = <$in>;
  close $in;

  my $data = parse_dxf ($filename, $dxf, $normalize_p, $wireframe_p, $layers_p);

  $filename = ($outfile eq '-' ? "<stdout>" : $outfile);
  my $code = generate_c ($infile, $filename, $smooth, $wireframe_p,
                         $normalize_p, $data);

  if ($outfile eq '-') {
    print STDOUT $code;
  } else {
    my $tmp = "$outfile.tmp";
    open (my $out, '>:utf8', $tmp) || error ("$tmp: $!");
    print $out $code || error ("$filename: $!");
    close $out || error ("$filename: $!");
    if (cmp_files ($filename, $tmp)) {
      if (!rename ($tmp, $filename)) {
        unlink $tmp;
        error ("mv $tmp $filename: $!");
      }
      print STDERR "$progname: wrote $filename\n";
    } else {
      unlink "$tmp" || error ("rm $tmp: $!\n");
      print STDERR "$progname: $filename unchanged\n" if ($verbose);
    }
  }
}


sub error() {
  ($_) = @_;
  print STDERR "$progname: $_\n";
  exit 1;
}

sub usage() {
  print STDERR "usage: $progname " .
        "[--verbose] [--normalize] [--smooth] [--wireframe] [--layers]\n" .
        "[infile [outfile]]\n";
  exit 1;
}

sub main() {
  my ($infile, $outfile);
  my $normalize_p = 0;
  my $smooth = 0;
  my $wireframe_p = 0;
  my $layers_p = 0;
  while ($_ = $ARGV[0]) {
    shift @ARGV;
    if ($_ eq "--verbose") { $verbose++; }
    elsif (m/^-v+$/) { $verbose += length($_)-1; }
    elsif ($_ eq "--normalize") { $normalize_p = 1; }
    elsif ($_ eq "--smooth") {
      if ($ARGV[0] && $ARGV[0] =~ m/^(\d[\d.]*)%?$/s) {
        $smooth = 0 + $1;
        shift @ARGV;
      } else {
        $smooth = 30;
      }
    }
    elsif ($_ eq "--wireframe") { $wireframe_p = 1; }
    elsif ($_ eq "--layers") { $layers_p = 1; }
    elsif (m/^-./) { usage; }
    elsif (!defined($infile)) { $infile = $_; }
    elsif (!defined($outfile)) { $outfile = $_; }
    else { usage; }
  }

  $infile  = "-" unless defined ($infile);
  $outfile = "-" unless defined ($outfile);

  dxf_to_gl ($infile, $outfile, $smooth, $normalize_p, $wireframe_p, $layers_p);
}

main;
exit 0;
