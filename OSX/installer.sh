#!/bin/bash
# XScreenSaver, Copyright © 2013-2022 Jamie Zawinski <jwz@jwz.org>
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

DEBUG=0
REQUIRED_SPACE=220	# MB. Highly approximate.

export PATH="/bin:/sbin:/usr/bin:/usr/sbin:$PATH"

function error() {
  echo "XScreenSaver Installer: Error: $@" >&2

  # Using "System Events" says "No user interaction allowed" on 10.9.
  # But using "SystemUIServer" or "Automator Runner" still seems to work.
  #
  runner="System Events"
  if [ -d "/System/Library/CoreServices/SystemUIServer.app" ]; then
    runner="SystemUIServer"
  elif [ -d "/System/Library/CoreServices/Automator Runner.app" ]; then
    runner="Automator Runner"
  fi

  (
    osascript <<__EOF__
      tell app "$runner" to \
        display dialog "$@" \
        buttons "Bummer" \
        default button 1 \
        with icon 0 \
        with title "Installation Error"
__EOF__
  ) </dev/null >/dev/null 2>&1 &
  exit 1
}


if [ x"$DSTVOLUME"    = x ]; then DSTVOLUME="/";              fi
if [ x"$PACKAGE_PATH" = x ]; then error "PACKAGE_PATH unset"; fi
if [ x"$HOME"         = x ]; then error "HOME unset";         fi


echo "Destination: $DSTVOLUME" >&2

#if [ x"$USER" = xjwz ]; then DEBUG=1; fi

if [ "$DEBUG" != 0 ]; then DSTVOLUME=/tmp; fi

SRC=`dirname "$PACKAGE_PATH"`/"Screen Savers"
DST1="$DSTVOLUME/Library/Screen Savers"
DST2="$DSTVOLUME/Applications"

# Because of Sparkle.framework weirdness, "XScreenSaverUpdater.app" is
# in the DMG as a compressed tar file instead of an app, and we unpack
# it when installing.  Without this, auto-updates won't work: If there's
# an .app there, Sparkle thinks that "XScreenSaverUpdater.app" is the
# thing it should be updating instead of "Install Everything.pkg".
#
UPDATER_SRC="XScreenSaver.updater"
UPDATER_DST="XScreenSaverUpdater.app"


cd "$SRC" || error "The 'Screen Savers' folder does not exist.

You can't copy the installer out of the Disk Image!"


free=`df -k "$DSTVOLUME" |
     tail -1 | head -1 | awk '{print $4}'`
need=$(( $REQUIRED_SPACE * 1024 ))
if [ "$free" -lt "$need" ]; then
 free=$(( $free / 1024 ))
 error "Not enough disk space: $free MB available, $REQUIRED_SPACE MB required."
else
 free=$(( $free / 1024 ))
 echo "Free space: $free MB" >&2
fi


mkdir -p "$DST1" || error "Unable to create directory $DST1/"
mkdir -p "$DST2" || error "Unable to create directory $DST2/"

# Install the savers and the updater in /System/Library/Screen Savers/
# Install the other apps in /Applications/
#
for f in *.{saver,app} "$UPDATER_SRC" ; do

  if [ ! -e "$f" ]; then continue; fi  # in case it is literally "*.app"

  EXT=`echo "$f" | sed 's/^.*\.//'`
  if [ "$f" = "$UPDATER_SRC" ]; then
    DST="$DST1"
  elif [ "$EXT" = "app" ]; then
    DST="$DST2"
  else
    DST="$DST1"
  fi

  DD="$DST/$f"

  echo "Installing $DD" >&2
  rm -rf "$DD" || error "Unable to delete $DD"

  if [ "$f" = "$UPDATER_SRC" ]; then
    echo "Unpacking  $DST/$UPDATER_DST" >&2
    # Need to delete the old one first or tar might refuse to replace
    # subdirectories with symbolic links.
    rm -rf "$DST/$UPDATER_DST"
    ( cd "$DST/" && tar -xzf - ) < "$f" ||
      error "Unable to unpack $f to $DST/$UPDATER_DST"
    f="$UPDATER_DST"
    DD="$DST/$f"
    EXT="app"
  else
    cp -pR "$f" "$DD" || error "Unable to install $f in $DST/"
  fi

  # The files in the DMG might be owned by "jwz:staff" (503:20)
  chown -R root:wheel "$DD"

  # Eliminate the "this was downloaded from the interweb" warning.
  # (This trick probably doesn't work.)
  xattr -r -d com.apple.quarantine "$DD"

  if [ "$EXT" = "app" ]; then
    # Eliminate the "this is from an unknown developer" warning.
    # (This trick probably doesn't work.)
    spctl --add "$DD"
  fi

  # If this saver or app is also installed in any per-user directory,
  # delete those copies so that we don't have conflicts.
  #
  if [ "$DEBUG" = 0 ]; then
    rm -rf "$DSTVOLUME/Users/"*"/Library/Screen Savers/$f"
  fi
done


# Savers are sandboxed as of 10.15, which means the preferences files
# moved.  If this is 10.15 or later, and there are old preferences files,
# move them to the new location.  Without this, all saver preferences
# would be wiped by the upgrade.
#
V=`sw_vers -productVersion`
V0=`echo $V | sed 's/^\([^.]*\).*/\1/'`
V1=`echo $V | sed 's/^[^.]*\.\([^.]*\).*$/\1/'`

if [ "$V0" -ge 11 -o \
     "$V0" -eq 10 -a "$V1" -ge 15 ] ; then	# If >= 10.15
  for HOME2 in "$DSTVOLUME/Users/"* ; do	# for each user

    CON="$HOME2/Library/Containers"
    LEG="$CON/com.apple.ScreenSaver.Engine.legacyScreenSaver"
    NBH="$LEG/Data/Library/Preferences/ByHost"
    OBH="$HOME2/Library/Preferences/ByHost"

    if [ -d "$CON" ]; then		# if there are Containers
      if ! [ -d "$LEG" ]; then		# but no legacyScreenSaver already
        UG=`stat -f %u:%g "$CON"`
        umask 077
        mkdir -p "$NBH"			# create tree
        chown -R "$UG" "$LEG"		# correct user/group
        echo "Moving old prefs to $NBH" >&2
        mv "$OBH/org.jwz.xscreensaver."* "$NBH/"
      fi
    fi
  done
fi


# Launch the updater so that the user gets these dialogs now, instead of them
# happening the first time a screen saver activates, and that screen saver
# failing to run and showing up black.  (I'm not sure if this is working.)
#
#   - "XScreenSaverUpdater was downloaded from the internet, are you sure?"
#   - "Rosetta 2 must be installed to run this software. Install it?”
#
su "$USER" -c "open \"$DST1/$UPDATER_DST\"" &
sleep 5


# Launch System Preferences with the "Desktop" pane selected.  In the olden
# days, this was a single pane with two tabs, "Desktop" and "Screen Saver",
# and it would always come up with "Desktop" selected.  As of macOS 13, these
# are two top-level pages, and this still opens the "Desktop" one.  There is
# still no way to open the "Screen Saver" page directly.  I'm guessing they
# are the same code, despite displaying two tabs.  Well, at least this scrolls
# the list of pages to *approximately* the right spot...
#
su "$USER" -c \
 "open /System/Library/PreferencePanes/DesktopScreenEffectsPref.prefPane" &

exit 0
