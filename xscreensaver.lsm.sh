#!/bin/sh
#
# generate an lsm file (http://sunsite.unc.edu/pub/Linux/Incoming/LSM-TEMPLATE)
# that is more-or-less correct for the current version of xscreensaver.
# jwz, 18-Jan-98

size() {
    ls -l $* |
    tail -1 |
    sed 's/.* \([0-9][0-9][0-9][0-9][0-9]*\) .*/\1/' |
    sed 's/[0-9][0-9][0-9]$/K/'
}

VERSION=`sed -n 's/.*\([0-9][0-9]*\.[0-9]*\).*/\1/p' < utils/version.h`
DATE=`date '+%d%b%y' | tr a-z A-Z`

ADIR=archive/
TAR_SIZE=`size ${ADIR}xscreensaver-$VERSION.tar.gz`
README_SIZE=`size README`
#LSM_SIZE=`size xscreensaver.lsm`
LSM_SIZE="1K"

echo "Begin3
Title:          xscreensaver
Version:        $VERSION
Entered-date:   $DATE
Description:    A modular screen saver and locker for the X Window System.
                Highly customizable: allows the use of any program that
                can draw on the root window as a display mode.
                More than 150 display modes are included in this package.
Keywords:       screen saver, screen lock, lock, xlock, X11
Author:         jwz@jwz.org (Jamie Zawinski)
Maintained-by:  jwz@jwz.org (Jamie Zawinski)
Primary-site:   http://www.jwz.org/xscreensaver/
                $TAR_SIZE xscreensaver-$VERSION.tar.gz
                $README_SIZE   xscreensaver.README
                $LSM_SIZE    xscreensaver.lsm
Alternate-site: sunsite.unc.edu /pub/Linux/X11/screensavers/
                $TAR_SIZE xscreensaver-$VERSION.tar.gz
                $README_SIZE   xscreensaver.README
                $LSM_SIZE    xscreensaver.lsm
Alternate-site: ftp.x.org /contrib/applications/
                $TAR_SIZE xscreensaver-$VERSION.tar.gz
                $README_SIZE   xscreensaver.README
                $LSM_SIZE    xscreensaver.lsm
Platforms:      Linux, Irix, SunOS, Solaris, HPUX, AIX, FreeBSD, NetBSD,
                BSDI, SCO, OSF1, Ultrix, VMS.
                Requires X11 and ANSI C.
                Works with GTK+, GNOME, and/or Motif.
                Shadow passwords, Kerberos, and OpenGL optionally supported.
                Multi-headed machines supported.
Copying-policy: BSD
End"

exit 0
