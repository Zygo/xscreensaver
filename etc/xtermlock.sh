#!/bin/sh

# Runs xlock displaying to the current X terminal emulator window
# Works well in xterm, but not so well in terminal emulators like
# dtterm that have menu bars and other junk.
#
# All arguments are passed to xlock.

g=`stty -a |
    sed -ne's/.* ypixels = \([^;]*\); xpixels = \([^;]*\).*/\2x\1/p'`
exec xlock -inwindow -parent $WINDOWID +install -geometry $g "$@"
