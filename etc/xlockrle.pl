#!/usr/bin/perl -T -w
# This is used  to add life to life
# This is a QUICK hack to convert life files to xlock's life format.
# Patterns MUST have <= 64 pts at start for life.c to use the data generated
# Below is an example of a life file without the first initial #'s
# Call the file piston.life and run it like xlocklife.pl < piston.life
#piston.life
##P -10 -3  Treated as a comment, program finds own center
#..........*...........
#..........****........
#**.........****.......
#**.........*..*.....**
#...........****.....**
#..........****........
#..........*...........

local($PTS, $X, $Y);

print "
Drop these points in life.c, within the 'patterns' array.
Note if the number of points > 64, one must increase points NUMPTS;
also to fit most screens and especially the iconified window,
one should have the size < 32x32.\n\n";
&search;
print "\npoints = $PTS; size = ${X}x$Y\n";

sub search {
    local ($row, $col, $firstrow, $firstcol);
    local ($i, $j, $found, $c, $tempx, $tempy);
    local (@array);


    $row = $col = 0;
    $firstrow = -1;
    $firstcol = 80;
    $PTS = $X = $Y = 0;
    while (<>) {
         if ((!($_ =~ /^#/)) && (!($_ =~ /^x/))) {
            @chars = split(//);
            $col = 0;
            $number = 0;
            foreach $c (@chars) {
                $col++;
                if ($c =~ /[1234567890]/) {
			$number = $number * 10 + ($c - '0');
                } elsif ($c =~ /[b]/) {
			if ($number == 0) {
                		printf ".";
			} else {
				for ($j = 0; $j < $number; $j++) {
                			printf ".";
				}
				$number = 0;
			}
		} elsif ($c =~ /[o]/) {
			if ($number == 0) {
                		printf "o";
    			} else {
				for ($j = 0; $j < $number; $j++) {
                			printf "o";
				}
				$number = 0;
			}
		} elsif ($c =~ /[\$]/) {
			if ($number == 0) {
                		printf "\n";
    			} else {
				for ($j = 0; $j < $number; $j++) {
                			printf "\n";
				}
				$number = 0;
			}
		} elsif ($c =~ /[\!]/) {
                	printf "\n";
			return;

		}
		$row++;
	    }
         }
  }
  print "127\n	},\n";
  $X = $col;
}
