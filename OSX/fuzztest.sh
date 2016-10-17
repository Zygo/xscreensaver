#!/bin/sh
# Copyright © 2016 Jamie Zawinski <jwz@jwz.org>
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.  No representations are made about the suitability of this
# software for any purpose.  It is provided "as is" without express or 
# implied warranty.
#
# Deliver random rotate and shake gestures to the iOS Simulator window.
#
# To make this work, you probably need to go to "System Preferences /
# Security & Privacy / Privacy / Accessibility" and add "Terminal.app"
# to the list of allowed programs.
#
# Created: 18-Apr-2016.

function menu() {
  which="$1"
  sim="Simulator"
 #proc="SystemUIServer"
  proc="System Events"

  osascript -e "
   tell application \"$sim\" to activate
   tell application \"$proc\"
    tell process \"$sim\"
     tell menu bar item \"Hardware\" of menu bar 1
      click menu item \"$which\" of menu \"Hardware\"
      \"$which\"
     end tell
    end tell
   end tell"
}

menu 'Shake Gesture'

while true; do
  i=$[ 2 + $[ RANDOM % 5 ]]
  echo "sleep $i" ; sleep $i
  i=$[ RANDOM % 5]
  if [ $i == 0 ]; then menu 'Shake Gesture'
  else
    i=$[ RANDOM % 3]
    if   [ $i == 0 ]; then menu 'Rotate Left'
    elif [ $i == 1 ]; then menu 'Rotate Right'
    else menu 'Rotate Right' ; menu 'Rotate Right'
    fi
  fi
done

exit 0
