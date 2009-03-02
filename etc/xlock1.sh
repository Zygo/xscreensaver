#!/bin/sh
#
# My lil' wrapper routine to execute a program, but only if it's
# not already running
# Sometimes xlock crashes when xautlock calls xlock when already running.
#
PROG=xlock

TST=`ps aux | grep $PROG | grep -v grep`
if [ -z "$TST" ]; then
	$PROG
fi
