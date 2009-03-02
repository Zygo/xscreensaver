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
# Revision History:
# 00-Jan-23  erase-modename threw it off, an extra space did the trick
#            Also works on Sun now.  David Bagley

# awk fails on Solaris but nawk is OK
if [ `uname` == "SunOS" ] ; then
	AWK="nawk"
else
	AWK="awk"
fi
#    gsub(/.*\] \[-mode/, ""); gsub(/\| /, ""); gsub("^ +", ""); \
# --help is a deliberate mistype...
function listmodes
{
  xlock --help 2>&1 | $AWK '{ \
    if (!true && match ($0,"-mode ")) { \
      gsub(/.*-mode /, ""); gsub(/\| /, ""); gsub("^ +", ""); \
      printf("%s ", $0); true = 1 \
    } else { if (true && /\|/) { \
      gsub(/\| /, ""); gsub("^ +", ""); gsub("\]$", ""); \
      printf("%s ", $0) } \
    } \
  }'
}
for i in `listmodes`; do echo Trying mode $i; xlock -nolock -mode $i; done
