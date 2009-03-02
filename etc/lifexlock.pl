#!/usr/bin/perl -T -w
# This is used  to reverse xlock format back to life

&search;

sub search {
    local ($row, $col, $NOTUSED, $x, $y);
    local (@array);

    $HALFMAX = 64; # really 32 but being safe
    $MAXCOL = $MINCOL = $MAXROW = $MINROW = $HALFMAX;
    $NOTUSED = -127;
    $row = $col = $NOTUSED;
    $number = 0;
    while (<>) {
        if (!($_ =~ /^#/)) {
            @chars = split(//);
	    $number = 0;
	    $col = $NOTUSED;
	    $negative = 1;
            foreach $c (@chars) {
                if ($c =~ /[-]/) {
		    $negative = -1;
		} elsif ($c =~ /[0123456789]/) {
		    $number = $number * 10 + ($c - '0');
		} elsif ($c =~ /[,]/) { # Last number does not have a ","
		    if ($col > $NOTUSED) {
		        $row = $number * $negative;
                        $col = $col + $HALFMAX;
			$row = $row + $HALFMAX;
                        $array{$col, $row} = 1;
		        if ($col > $MAXCOL) {
			    $MAXCOL = $col;
		        } elsif ($col < $MINCOL) {
			    $MINCOL = $col;
                        }
		        if ($row > $MAXROW) {
 			    $MAXROW = $row;
		        } elsif ($row < $MINROW) {
			    $MINROW = $row;
                        }
	                $col = $NOTUSED;
		    } else {
			$col = $number * $negative;
                    }
		    $number = 0;
	    	    $negative = 1;
		} elsif ($c =~ /[{}\/]/) { # Last number does not have a ","
	            $col = $NOTUSED;
		    $number = 0;
	    	    $negative = 1;
                }
            }
        }
    }
    $x=$MAXCOL - $MINCOL + 1;
    $y=$MAXROW - $MINROW + 1;
    print "#x=$x, y=$y\n";
    for ($j = $MINROW; $j <= $MAXROW; $j++) {
        for ($i = $MINCOL; $i <= $MAXCOL; $i++) {
            if ($array{$i, $j}) {
                print "*";
 	    } else {
                print ".";
            }
        }
        print "\n";
    }
}
