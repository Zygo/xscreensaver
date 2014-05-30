#!/bin/sh
# XScreenSaver, Copyright © 2013 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# The guts of the installer.  Copies the screen savers out of the adjacent
# "Screen Savers" directory and into "/Library/Screen Savers/".  We do it
# this way instead of just including the screen savers in the package
# because that would double the size of the DMG.
#
# Created: 27-Jul-2013.

#exec >/tmp/xscreensaver.log 2>&1
#set -x

export PATH="/bin:/sbin:/usr/bin:/usr/sbin:$PATH"

REQUIRED_SPACE=140	# MB. Highly approximate.

DEBUG=0

if [ x"$USER" = xjwz ]; then
  DEBUG=1
fi

echo "Destination: $DSTVOLUME" >&2

if [ "$DEBUG" != 0 ]; then
  DSTVOLUME=/tmp
fi

SRC=`dirname "$PACKAGE_PATH"`/"Screen Savers"
DST1="$DSTVOLUME/Library/Screen Savers"
DST2="$DSTVOLUME/Applications"
PU="$DSTVOLUME/$HOME/Library/Screen Savers"
UPDATER="XScreenSaverUpdater.app"

function error() {
  echo "Error: $@" >&2
  (
    osascript <<__EOF__
      tell app "System Events" to display dialog "$@" buttons "Bummer" default button 1 with icon 0 with title "Installation Error"
__EOF__
  ) </dev/null >/dev/null 2>&1 &
  exit 1
}

cd "$SRC" || error "The 'Screen Savers' folder does not exist.

You can't copy the installer out of the Disk Image!"


free=`df -k "$DSTVOLUME" |
     tail -1 | head -1 | awk '{print $4}'`
need=`echo $REQUIRED_SPACE \* 1024 | bc`
if [ "$free" -lt "$need" ]; then
 free=`echo $free / 1024 | bc`
 error "Not enough disk space: $free MB available, $REQUIRED_SPACE MB required."
fi


mkdir -p "$DST1" || error "Unable to create directory $DST1/"
mkdir -p "$DST2" || error "Unable to create directory $DST2/"

# Install the savers and the updater in /System/Library/Screen Savers/
# Install the other apps in /Applications/
#
for f in *.{saver,app} ; do
  EXT=`echo "$f" | sed 's/^.*\.//'`
  if [ "$EXT" = "app" -a "$f" != "$UPDATER" ]; then
    DST="$DST2"
  else
    DST="$DST1"
  fi
  DD="$DST/$f"
  echo "Installing $DD" >&2
  rm -rf "$DD" || error "Unable to delete $DD"
  cp -pR "$f" "$DST/" || error "Unable to install $f in $DST/"

  # Eliminate the "this was downloaded from the interweb" warning.
  xattr -r -d com.apple.quarantine "$DD"

  if [ "$EXT" = "app" ]; then
    # Eliminate the "this is from an unknown developer" warning.
    spctl --add "$DD"
  fi

  # If this saver or app is also installed in the per-user directory,
  # delete that copy so that we don't have conflicts.
  #
  if [ "$DEBUG" = 0 ]; then
    rm -rf "$PU/$f"
  fi
done

# Launch System Preferences with the Screen Saver pane selected.
#
open /System/Library/PreferencePanes/DesktopScreenEffectsPref.prefPane &

exit 0
