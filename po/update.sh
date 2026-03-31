#!/bin/sh

# syncs the .po files with the version in GNOME CVS.
# The URL below is updated every ~4 hours.

url="http://developer.gnome.org/projects/gtp/status/gnome-2.0-extras/po/xscreensaver.HEAD."

for f in *.po
do
  f2="$f.tmp"
  rm -f "$f2"
  echo -n "$f ... "
  wget -q -O"$f2" "$url$f"
  if [ ! -s "$f2" ]; then
    echo "not found."
  elif ( cmp -s "$f" "$f2" ); then
    echo "unchanged."
  else
    cat "$f2" > "$f"
    echo "updated."
  fi
  rm -f "$f2"
done
exit 0
