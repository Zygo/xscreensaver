#!/bin/ksh

# or /bin/bash

# Simple script to look all xlock modes supported.
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted,
# provided that the above copyright notice appear in all copies and that
# both that copyright notice and this permission notice appear in
# supporting documentation.
#
# This file is provided AS IS with no warranties of any kind.  The author
# shall have no liability with respect to the infringement of copyrights,
# trade secrets or any patents by this file or any part thereof.  In no
# event will the author be liable for any lost revenue or profits or
# other special, indirect and consequential damages.
#
# xlock-show-modes Copyright (C)  1998 Andrea Arcangeli
# 		by Andrea Arcangeli <arcangeli@mbox.queen.it>
#
# Does not work on my Sun (DAB)
function listmodes
{
	xlock --help 2>&1 | \
	awk '{ if (!true && match ($0,"-mode")) { gsub(/.*-mode/, ""); gsub(/\| /, ""); gsub("^ +", ""); printf("%s ", $0); true = 1 } else { if (true && /\|/) { gsub(/\| /, ""); gsub("^ +", ""); gsub("\]$", ""); printf("%s ", $0) } } }'
}

for i in `listmodes`; do echo Trying mode $i; xlock -nolock -mode $i; done
