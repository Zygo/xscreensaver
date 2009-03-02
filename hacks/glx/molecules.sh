#!/bin/sh

TARGET=$1
shift
SRCS=$*

TMP=molecules.h.$$
rm -f $TMP

if [ -z "$UTILS_SRC" ]; then UTILS_SRC="../../utils"; fi

for f in $SRCS ; do
  sh "$UTILS_SRC/ad2c" "$f" |
    sed 's/",$/\\n"/' >> $TMP
  echo ',' >> $TMP
done
mv $TMP $TARGET
