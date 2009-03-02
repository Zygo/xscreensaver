#!/bin/sh

# Runs xlock displaying in another X client's window.
# use -me to use current terminal emulator window (works in ddterm).
# use -id <windowid> to display to a specific window.
# Otherwise select the target window with the cross-hair cursor.
# 
# All other arguments are passed to xlock.

case $1 in
-me)if [ "$WINDOWID" ]; then id="-id $WINDOWID"; shift
    else echo >&2 "WINDOWID not set"; exit 1; fi;;
-id)if [ $# -gt 1 ]; then id="-id $2"; shift; shift
    else echo >&2 "ID not specified"; exit 1; fi;;
esac
exec xlock  +install -inwindow -parent `xwininfo $id -int |
    awk '/Window id/{printf "%d", $4};
         /Width/{printf " -geometry %d", $2};
         /Height/{printf "x%d", $2}'` "$@"
