#!/bin/sh

TARGET=$1
shift
SRCS=$*

TMP=molecules.h.$$
rm -f $TMP
trap "rm -f $TMP" 1 2 3 15 ERR EXIT

if [ -z "$UTILS_SRC" ]; then UTILS_SRC="../../utils"; fi

for f in $SRCS ; do
  sh "$UTILS_SRC/ad2c" "$f" |
    sed 's/",$/\\n"/' >> $TMP
  echo ',' >> $TMP
done

if cmp -s $TMP $TARGET ; then
  rm $TMP
else
  mv $TMP $TARGET
fi
