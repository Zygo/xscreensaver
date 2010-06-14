#!/bin/sh

TARGET=$1
shift
SRCS=$*

TMP1=m6502.h.1.$$
TMP2=m6502.h.2.$$
rm -f $TMP1 $TMP2

if [ -z "$UTILS_SRC" ]; then UTILS_SRC="../utils"; fi

for f in $SRCS ; do
  sed 's/[ 	]*;.*$//' < "$f" > $TMP1  # lose comments
  sh "$UTILS_SRC/ad2c" $TMP1 |
    sed 's/",$/\\n"/' >> $TMP2
  echo ',' >> $TMP2
done
rm $TMP1
mv $TMP2 $TARGET
