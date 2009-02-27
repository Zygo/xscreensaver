#ifndef lint
static char sccsid[] = "@(#)logout.c	2.7 95/02/21 xlockmore";
#endif
/*
 * logout.c - handle logout from xlock
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 13-Feb-95: Heath A. Kehoe <hakehoe@icaen.uiowa.edu>.
 *            Mostly taken from bomb.c
 * 1994:      bomb.c written.  Copyright (c) 1994 Dave Shield
 *            Liverpool Computer Science
 */
#include <stdio.h>
#include <stdlib.h>
#ifdef SYSLOG
#include <syslog.h>
#endif
#include <sys/signal.h>
#include "xlock.h"

/*
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
 * on it's merry way (only applies if AUTO_LOGOUT or LOGOUT_BUTTON is
 * defined.)
 */

#define NAP_TIME        5               /* Sleep between shutdown attempts */

/* Seems to hang on Solaris, logs out but does not return login prompt */
void ugly_logout()
{

#ifndef KILL_ALL_OTHERS
#define KILL_ALL_OTHERS -1
    (void) signal(SIGHUP, SIG_IGN);
    (void) signal(SIGTERM, SIG_IGN);
#endif

    (void) kill( KILL_ALL_OTHERS, SIGHUP );
    (void) sleep(NAP_TIME);
    (void) kill( KILL_ALL_OTHERS, SIGTERM );
    (void) sleep(NAP_TIME);

#ifdef SYSLOG
    syslog(LOG_NOTICE, "xlock failed to exit - sending kill (uid %d)\n",
    getuid());
#endif

    (void) kill( KILL_ALL_OTHERS, SIGKILL );
    (void) sleep(NAP_TIME);

#ifdef SYSLOG
    syslog(LOG_WARNING, "xlock still won't exit - suicide (uid %d)\n",
    getuid());
#endif

    (void) kill(getpid(), SIGKILL);
    exit(-1);

}

/* Decide which routine you want with a 1 or 0 */
#if 1

void logoutUser()
{

#ifdef SYSLOG
    syslog(LOG_INFO, "xlock expired. closing down (uid %d) on %s\n",
		getuid(), getenv("DISPLAY"));
#endif

    ugly_logout();
}

#else

/* sample logoutUser (described above) */

void logoutUser()
{
    char *pidstr;
    int pid;

#ifdef SYSLOG
    syslog(LOG_INFO, "xlock expired. closing down (uid %d) on %s\n",
              getuid(), getenv("DISPLAY"));
#endif

    pidstr = getenv("XSESSION");
    if(pidstr) {
	pid = atoi(pidstr);
	kill(pid, SIGTERM);
	sleep(5);
    }
/*    ugly_logout(); */
    (void) sleep(1);
}

#endif


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
 
#ifndef STAFF_FILE
#define STAFF_FILE      "/usr/remote/etc/xlock.staff"
#endif
 
char    *staff_users[] = {      /* List of users allowed to lock */
"root",
0
};
 
char    *staff_groups[] = {     /* List of groups allowed to lock */
0
};
 
#undef passwd
#undef pw_name
#undef getpwnam
#undef getpwuid
#include <pwd.h>
#include <grp.h>

int full_lock()
{
    uid_t    uid;
    gid_t    gid;

    struct passwd *pwp;
    struct group  *gp;
    FILE   *fp;
 
    char        **cpp;
    char        buf[BUFSIZ];
 
    if ((uid = getuid()) == 0)
        return(FULL_LOCK);              /* root */
    pwp=getpwuid(uid);
 
    gid=getgid();
    gp=getgrgid( gid );

    if (inwindow || inroot || nolock) 
        return(FULL_LOCK);  /* (mostly) harmless user */

    for ( cpp = staff_users ; *cpp != NULL ; cpp++ )
        if (!strcmp(*cpp, pwp->pw_name))
            return(FULL_LOCK);  /* Staff user */
 
    for ( cpp = staff_groups ; *cpp != NULL ; cpp++ )
        if (!strcmp(*cpp, gp->gr_name))
            return(FULL_LOCK);  /* Staff group */
 
    if ((fp=fopen( STAFF_FILE, "r")) == NULL )
        return(TEMP_LOCK);
 
    while ((fgets( buf, BUFSIZ, fp )) != NULL ) {
        char    *cp;
 
        if ((cp = (char *) strchr(buf, '\n')) != NULL )
            *cp = '\0';
        if (!strcmp(buf, pwp->pw_name) || !strcmp(buf, gp->gr_name))
            return(FULL_LOCK);  /* Staff user or group */
    }
 
    return(TEMP_LOCK);
}
