#!/bin/sh

TARGET=$1
shift
SRCS=$*

TMP1=m6502.h.1.$$
TMP2=m6502.h.2.$$
rm -f $TMP1 $TMP2
trap "rm -f $TMP1 $TMP2" 1 2 3 15 ERR EXIT

if [ -z "$UTILS_SRC" ]; then UTILS_SRC="../utils"; fi

for f in $SRCS ; do
  sed 's/[ 	]*;.*$//' < "$f" > $TMP1  # lose comments
  sh "$UTILS_SRC/ad2c" $TMP1 |
    sed 's/",$/\\n"/' >> $TMP2
  echo ',' >> $TMP2
done
rm -f $TMP1
if cmp -s $TMP2 $TARGET ; then
  rm -f $TMP2
else
  mv $TMP2 $TARGET
fi

