#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)logout.c	4.02 97/04/01 xlockmore";

#endif

/*-
 * logout.c: handle compile-time optional logout
 *
 * See xlock.c for copying information.
 *
 * xclosedown code
 * Copyright 1990 by Janet Carson
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  The author makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * Revision History:
 * 27-Jul-95: put back in logout.c (oops).
 *            Window shutdown program by Janet L. Carson,
 *            Baylor College of Medicine.
 *            Main procedure modified for use w/ xlock by
 *            Anthony Thyssen <anthony@cit.gu.edu.au>.
 * 24-Feb-95: fullLock rewritten to handle non-default group names from
 *            Dale A. Harris <rodmur@ecst.csuchico.edu>
 * 13-Feb-95: Heath A. Kehoe <hakehoe@icaen.uiowa.edu>.
 *            Mostly taken from bomb.c
 * 1994:      bomb.c written.  Copyright (c) 1994 Dave Shield
 *            Liverpool Computer Science
 */

#include "xlock.h"

#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT ) || defined( USE_BOMB )

#if ( HAVE_SYSLOG_H && defined( USE_SYSLOG ))
#include <syslog.h>
#endif
#include <sys/signal.h>

extern Bool inroot, inwindow, nolock, debug;
extern char *ProgramName;
extern char *logoutCmd;

/*-
 * This file contains a function called logoutUser() that, when called,
 * will (try) to log out the user.
 *
 * A portable way to do this is to simply kill all of the user's processes,
 * but this is a really ugly way to do it (it kills background jobs that
 * users may want to remain running after they log out).
 *
 * If your system provides for a cleaner/easier way to log out a user,
 * you may implement it here.
 *
 * For example, on some systems, one may define for the users an environment
 * variable named XSESSION that contains the pid of the X session leader
 * process.  So, to log the user out, we just need to kill that process,
 * and the X session will end.  Of course, a user can defeat that by
 * changing the value of XSESSION; so we can fall back on the ugly_logout()
 * method.
 *
 * If you can't log the user out (and you don't want to use the brute
 * force method) simply return from logoutUser(), and xlock will continue
 * on it's merry way (only applies if USE_AUTO_LOGOUT or USE_BUTTON_LOGOUT
 * is defined.)
 */

#define NAP_TIME        5	/* Sleep between shutdown attempts */


#ifdef CLOSEDOWN_LOGOUT

/* Logout the user by contacting the display and closeing all windows */


/*-
 *  Window shutdown program by Janet L. Carson, Baylor College of Medicine.
 *  Version 1.0, placed in /contrib on 2/12/90.
 *
 *  Please send comments or fixes to jcarson@bcm.tmc.edu
 */

/*-
 *  I'm probably going to get some BadWindow errors as I kill clients
 *  which have multiple windows open and then try to kill them again
 *  on another of their windows.  I'm just going to plow right through!
 *  The flag is set back to false in recurse_tree and kill_tree...
 */

static int  err_occurred = False;

static int
err_handler(Display * display, XErrorEvent * err)
{
	err_occurred = True;
	return 0;
}

/*-
 *  Looking for properties...
 */

static int
has_property(Display * display, Window window, Atom prop)
{
	int         nprops, j, retval = 0;
	Atom       *list = XListProperties(display, window, &nprops);

	if (err_occurred)
		return 0;

	for (j = 0; j < nprops; j++) {
		if (list[j] == prop) {
			retval = 1;
			break;
		}
	}

	if (nprops)
		XFree((caddr_t) list);

	return retval;
}

/*-
 *  Send a WM_PROTOCOLS WM_DELETE_WINDOW message to a window
 */

static void
send_delete_message(Display * display, Window window,
		    Atom protocols_atom, Atom delete_window_atom)
{
	XClientMessageEvent xclient;

	xclient.type = ClientMessage;
	xclient.send_event = True;
	xclient.display = display;
	xclient.window = window;
	xclient.message_type = protocols_atom;
	xclient.format = 32;
	xclient.data.l[0] = delete_window_atom;

	XSendEvent(display, window, False, 0, (XEvent *) & xclient);
}

/*-
 *  To shutdown a top level window:  if the window participates
 *  in WM_DELETE_WINDOW, let the client shut itself off.  Otherwise,
 *  do an XKillClient on it.
 */

static void
handle_top_level(Display * display, Window window,
		 Atom protocols_atom, Atom delete_window_atom)
{
	Atom       *prots;
	int         nprots, j;

	if (has_property(display, window, protocols_atom)) {
		XGetWMProtocols(display, window, &prots, &nprots);

		if (err_occurred)
			return;

		for (j = 0; j < nprots; j++)
			if (prots[j] == delete_window_atom) {
				send_delete_message(display, window,
					 protocols_atom, delete_window_atom);
				break;
			}
		if (j == nprots)	/* delete window not found */
			XKillClient(display, window);

		XFree((caddr_t) prots);
	} else
		XKillClient(display, window);
}

/*-
 *  recurse_tree: look for top level windows to kill all the way down
 *  the window tree.  This pass is "nice"--I'll use delete_window protocol
 *  if the window supports it.  If I get an error in the middle, I'll start
 *  over again at the same level, because reparenting window managers throw
 *  windows back up to the root...
 */

static void
recurse_tree(Display * display, Window window,
	     Atom state_atom, Atom protocols_atom, Atom delete_window_atom)
{
	Window      root, parent, *kids;
	unsigned int nkids;
	int         j;
	int         swm_state;

	for (;;) {
		XQueryTree(display, window, &root, &parent, &kids, &nkids);
		if (err_occurred) {
			err_occurred = False;
			return;
		}
		for (j = 0; j < nkids; j++) {
			swm_state = has_property(display, kids[j], state_atom);

			if (err_occurred)
				break;

			if (swm_state) {
				handle_top_level(display, kids[j], protocols_atom, delete_window_atom);
				if (err_occurred)
					break;
			} else
				recurse_tree(display, kids[j],
					     state_atom, protocols_atom, delete_window_atom);
		}

		XFree((caddr_t) kids);

		/* when I get all the way through a level without an error, I'm done  */

		if (err_occurred)
			err_occurred = False;
		else
			return;

	}
}

/*-
 *  This is the second pass--anything left gets an XKillClient!
 */

static void
kill_tree(Display * display, Window window)
{
	Window      root, parent, *kids;
	unsigned int nkids;
	int         j;

	for (;;) {
		XQueryTree(display, window, &root, &parent, &kids, &nkids);
		if (err_occurred) {
			err_occurred = False;
			return;
		}
		for (j = 0; j < nkids; j++) {
			XKillClient(display, kids[j]);
			if (err_occurred)
				break;
		}

		XFree((caddr_t) kids);

		/* when I get all the way through a level without an error, I'm done  */

		if (err_occurred)
			err_occurred = False;
		else
			return;
	}
}

/*-
 *  Main program
 */

static void
closedownLogout(Display * display)
{
	Atom        __SWM_DELETE_WINDOW = None;
	Atom        __SWM_PROTOCOLS = None;
	Atom        __SWM_STATE = None;
	int         j;

#if 0
	/* synchronize -- so I'm aware of errors immediately */
	XSynchronize(display, True);

	/* use my error handler from here on out */
	(void) XSetErrorHandler(err_handler);
#endif

	/* init atoms */
	__SWM_STATE = XInternAtom(display, "__SWM_STATE", False);
	__SWM_PROTOCOLS = XInternAtom(display, "__SWM_PROTOCOLS", False);
	__SWM_DELETE_WINDOW = XInternAtom(display, "__SWM_DELETE_WINDOW", False);

	/* start looking for windows to kill -- be nice on pass 1 */
	for (j = 0; j < ScreenCount(display); j++)
		recurse_tree(display, RootWindow(display, j),
			  __SWM_STATE, __SWM_PROTOCOLS, __SWM_DELETE_WINDOW);

	/* wait for things to clean themselves up */
	(void) sleep(NAP_TIME);

	/* this will forcibly kill anything that's still around --
	   this second pass may or may not be needed... */
	for (j = 0; j < ScreenCount(display); j++)
		kill_tree(display, RootWindow(display, j));
	(void) sleep(NAP_TIME);
}

#endif /* CLOSEDOWN_LOGOUT */

#ifdef SESSION_LOGOUT
static void
sessionLogout(void)
{
	char       *pidstr;

	pidstr = getenv("XSESSION");
	if (pidstr) {
		kill(atoi(pidstr), SIGTERM);
		(void) sleep(NAP_TIME);
	}
}

#endif /* SESSION_LOGOUT */

static void
uglyLogout(void)
{
#if defined(__cplusplus) || defined(c_plusplus)
	extern int  signal(int, void *);
	extern int  kill(int, int);

#endif

#ifndef VMS
#ifndef KILL_ALL_OTHERS
#define KILL_ALL_OTHERS -1
	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGTERM, SIG_IGN);
#endif

	(void) kill(KILL_ALL_OTHERS, SIGHUP);
	(void) sleep(NAP_TIME);
	(void) kill(KILL_ALL_OTHERS, SIGTERM);
	(void) sleep(NAP_TIME);

#if ( HAVE_SYSLOG_H && defined( USE_SYSLOG ))
	syslog(SYSLOG_NOTICE, "%s: failed to exit - sending kill (uid %d)\n",
	       ProgramName, getuid());
#endif

	(void) kill(KILL_ALL_OTHERS, SIGKILL);
	(void) sleep(NAP_TIME);

#if ( HAVE_SYSLOG_H && defined( USE_SYSLOG ))
	syslog(SYSLOG_WARNING, "%s: still won't exit - suicide (uid %d)\n",
	       ProgramName, getuid());
#endif

	(void) kill(getpid(), SIGKILL);
#endif /* !VMS */
	exit(-1);
}

void
logoutUser(Display * display)
{

#if ( HAVE_SYSLOG_H && defined( USE_SYSLOG ))
	extern      syslogStop(char *);

	syslog(SYSLOG_INFO, "%s: expired. closing down (uid %d) on %s\n",
	       ProgramName, getuid(), getenv("DISPLAY"));
	syslogStop(XDisplayString(display));
#endif
	if (logoutCmd && *logoutCmd) {
		int         cmd_pid;

		if ((cmd_pid = FORK()) == -1) {
			(void) fprintf(stderr, "Failed to launch \"%s\"\n", logoutCmd);
			perror(ProgramName);
			cmd_pid = 0;
		} else if (!cmd_pid) {
			(void) system(logoutCmd);
			exit(0);
		}
	}
#ifdef CLOSEDOWN_LOGOUT
	(void) finish(display, False);
#else
	(void) finish(display, True);
#endif
#ifdef VMS
	(void) system("mcr decw$endsession -noprompt");
#else
#ifdef __sgi
	(void) system("/usr/bin/X11/tellwm end_session >/dev/null 2>&1");
	(void) sleep(10);	/* Give the above a chance to run */
#endif
#endif

#ifdef CLOSEDOWN_LOGOUT
	/* Do not want to kill other user's processes like if telnetted in */
	closedownLogout(display);
	return;
#endif
#ifdef SESSION_LOGOUT
	sessionLogout();
#endif
	uglyLogout();
	exit(-1);
}


#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT )
	/*
	 *  Determine whether to "fully" lock the terminal, or
	 *    whether the time-limited version should be imposed.
	 *
	 *  Policy:
	 *      Members of staff can fully lock
	 *        (hard-wired user/group names + file read at run time)
	 *      Students (i.e. everyone else)
	 *          are forced to use the time-limit version.
	 *
	 *  An alternative policy could be based on display location
	 */
#define FULL_LOCK       1
#define TEMP_LOCK       0

/* assuming only staff file is needed */
#ifndef STAFF_FILE
#define STAFF_FILE      "/usr/remote/etc/xlock.staff"
#endif

#undef passwd
#undef pw_name
#undef getpwnam
#undef getpwuid
#include <pwd.h>
#include <grp.h>
#include <sys/param.h>
#include <errno.h>
#ifndef NGROUPS
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef NGROUPS_MAX
#define NGROUPS NGROUPS_MAX
#else
#define NGROUPS NGROUPS_MAX_DEFAULT
#endif
#endif

int
fullLock(void)
{
	uid_t       uid;
	int         ngroups = NGROUPS;

#ifdef SUNOS4
	gid_t       mygidset[NGROUPS * sizeof (gid_t)];

#else
	gid_t       mygidset[NGROUPS];

#endif
	int         ngrps, i;
	struct passwd *pwp;
	struct group *gp;
	FILE       *fp;
	char        buf[BUFSIZ];

	uid = getuid();
	/* Do not try to logout root! */
	if (!uid)
		return (FULL_LOCK);	/* root */
	if (inwindow || inroot || nolock /*|| debug */ )
		return (FULL_LOCK);	/* (mostly) harmless user */

	pwp = getpwuid(uid);
	if ((ngrps = getgroups(ngroups, mygidset)) == -1)
		perror(ProgramName);

#ifdef STAFF_NETGROUP
	if (innetgr(STAFF_NETGROUP, NULL, pwp->pw_name, NULL))
		return (FULL_LOCK);
#endif

	if ((fp = my_fopen(STAFF_FILE, "r")) == NULL)
		return (TEMP_LOCK);

	while ((fgets(buf, BUFSIZ, fp)) != NULL) {
		char       *cp;

		if ((cp = (char *) strchr(buf, '\n')) != NULL)
			*cp = '\0';
		if (!strcmp(buf, pwp->pw_name))
			return (FULL_LOCK);
		if ((gp = getgrnam(buf)) != NULL) {
			/* check all of user's groups */
#ifdef SUNOS4
			for (i = 1; i < ngrps * sizeof (gid_t); i += 2)
#else
			for (i = 0; i < ngrps; ++i)
#endif
				if (gp->gr_gid == mygidset[i])
					return (FULL_LOCK);
		}
	}
	(void) fclose(fp);

	return (TEMP_LOCK);
}
#endif /* USE_AUTO_LOGOUT || USE_BUTTON_LOGOUT */

#endif /* USE_AUTO_LOGOUT || USE_BUTTON_LOGOUT || USE_BOMB */
