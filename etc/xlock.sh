#!/bin/sh -
# Wrapper script to get better performance
#	by Tim Auckland <Tim.Auckland@Sun.COM>
# It simply suspends all netscape and JAVA applications and resumes them
# once xlock exits.  These applications poll when idle and that is quite
# inconsiderate when you need all the cycles you can get for a good
# screensaver.
jobs=`ps -u $USER| awk '/jre/||/netscape/{print $1}'`
[ -n "$jobs" ] && trap 'kill -CONT $jobs' 0 1 2 14 15 && kill -STOP $jobs
/usr/local/bin/xlock "$@"
