#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)vtlock.c	1.3 2000/01/28 xlockmore";

#endif

/* Copyright (c) E. Lassauge/ R. Cohen-Scali, 1998. */

/*
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * The original code for was written by R. Cohen-Scali
 * (remi.cohenscali@pobox.com) for a command line vtswich control tool
 * can be found in the etc directory!
 *
 * My e-mail address is lassauge@mail.dotcom.fr
 *
 * REVISION HISTORY:
 *       98/10/01: Eric Lassauge - vtlock renamed from lockvt.
 *                 Merge changes from Remi and David.
 *       98/09/07: Remi Cohen-Scali - A problem stayed in the vtlock process:
 * 	If a user has switched vt while an xautolock is running, it could
 * 	be possible that the vt is locked. Then the user cannot unlock
 * 	vt switch without the vtswitch command I wrote to test.
 * 	(if you want it just send me a mail to remi.cohenscali@pobox.com
 * 	and I will send it back).
 * 	In order to avoid it, we need to know the active vt (this is easily
 * 	achieved with VT_GETSTATE ioctl), and, more dificult, we need to
 * 	know the vt used by the X server.The vt used by X is known as an
 *      internal variable in the xf86Info structure (xf86InfoRec defined in
 *	xc/programs/Xserver/hw/xfree86/common/xf86Priv.h).
 * 	The problem is that this structure in not accessible to the clients.
 * 	In order to access this information, the workaround is to use
 * 	the proc filesystem.
 * 	All proc specific routines are implemented in vtlock_proc.c
 * 	in order to help porting to other procfs.
 * 	The method is explained in the vtlock_proc.c file.
 *
 *	PS from E.L: all this stuff is significant for Linux only. For other
 *      OSs the problem remains the same but the details are different !
 */


#include "xlock.h"

#ifdef USE_VTLOCK

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#if defined( __linux__ )
#include <linux/vt.h>		/* for VT_LOCKSWITCH/VT_UNLOCKSWITCH */
#else
#error Sorry ! You must adapt this file to your system !
#endif


#define CONSOLE "/dev/console"

#define DISPLAY_NR(name)        (*(strchr( (name), ':' ) +1) -'0')

int         vtlocked = 0;
int         myvt = 0;

static uid_t ruid;
static int display_nr = -1;

	/* from vtlock_proc.c */
extern int is_x_vt_active(int);
extern int restore_vt_active(int);
extern int set_x_vt_active(int);
	/* from ressource.c */
extern Bool vtlock_set_active;
extern Bool vtlock_restore;
extern Bool debug;

extern void dovtlock(void);
extern void dovtunlock(void);

static void
getrootprivs(void)
{
        ruid = getuid();

        (void) seteuid(0);
}

/* revoke root privs, if there were any */
static void
revokerootprivs(void)
{
        (void) seteuid(ruid);
}

static void
lockvt(Bool lock)
{
        int         consfd = -1;
        struct stat consstat;

        if (stat(CONSOLE, &consstat) == -1 || ruid != consstat.st_uid) return;

        /* Open console */
        if ((consfd = open(CONSOLE, O_RDWR)) == -1) return;

        /* Do it */
        if (ioctl(consfd, lock ? VT_LOCKSWITCH : VT_UNLOCKSWITCH) == -1) {
                close(consfd);
                return;
        }
        /* Terminate */
        close(consfd);
        vtlocked = lock;
}

void
dovtlock(void)
{
    char *dispname = XDisplayName( (char *)NULL );
    Bool x_vt_active = 0;

    if (debug)
      (void) fprintf(stderr,"dovtlock start\n");
    if ( display_nr == -1 ) {
      if ( dispname )
        display_nr = DISPLAY_NR( dispname );
      else
        display_nr = 0;
	}
    getrootprivs();
    x_vt_active = is_x_vt_active(display_nr);
    if ( ! x_vt_active ) {
        if ( vtlock_set_active )
          if ( set_x_vt_active( display_nr ) != -1 )
            lockvt(True);
    }
    else
      lockvt(True);
    if (debug)
      (void) fprintf(stderr,"dovtlock: %d\n",vtlocked);

    revokerootprivs();
}

void
dovtunlock(void)
{
    char *dispname = XDisplayName( (char *)NULL );

    if (debug)
      (void) fprintf(stderr,"dovtunlock start\n");
    if ( display_nr == -1 ) {
      if ( dispname )
        display_nr = DISPLAY_NR( dispname );
      else
        display_nr = 0;
	}
    getrootprivs();
    if (is_x_vt_active(display_nr))
      lockvt(False);
    if ( vtlock_set_active && vtlock_restore )
      (void)restore_vt_active(display_nr);
    revokerootprivs();
}

#endif
