#ifndef lint
static char sccsid[] = "@(#)bomb.c	2.7 95/02/21 xlockmore";
#endif
/*
 * bomb.c - temporary screen lock for public labs
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 09-Jan-95: Assorted defines to control various aspects of bomb mode.
 *            Uncomment, or otherwise define the appropriate line
 *            to obtain the relevant behaviour, thanks to Dave Shield
 *            <D.T.Shield@csc.liv.ac.uk>.
 * 20-Dec-94: Time patch for multiprocessor machines (ie. Sun10) thanks to
 *            Nicolas Pioch <pioch@Email.ENST.Fr>.
 * 1994:      Written.  Copyright (c) 1994 Dave Shield
 *            Liverpool Computer Science
 */
 
#include "xlock.h"
 
#include <sys/signal.h>
#ifdef SYSLOG
#include <syslog.h>
#endif
#include <stdio.h>
 
/* #define SIMPLE_COUNTDOWN *//* Display a simple integer countdown,     */
                              /*  rather than a "MIN:SEC" format.        */
#define COLOUR_CHANGE         /* Display cycles through the colour wheel */
                              /*  rather than staying red throughout.    */

#define FULL_COUNT_FONT         "-*-*-*-*-*-*-34-*-*-*-*-*-*-*"
#define ICON_COUNT_FONT         "-*-*-*-*-*-*-8-*-*-*-*-*-*-*"
#define COUNTDOWN       600             /* No. seconds to lock for */
#define NDIGITS         4               /* Number of digits in count */

#define MAX_DELAY       1000000         /* Max delay between updates */
#define DELTA           10              /* Border around the digits */
#define RIVET_RADIUS    6               /* Size of detonator 'rivets' */
 
void rivet();
void explode();
extern long allocpixel();
 
typedef struct {
    int width, height;
    int x, y;
    int delta;
    int countdown;
    int text_width;
    int text_ascent;
    int text_descent;
}       bombstruct;
 
static bombstruct  bombs[MAXSCREENS];

void
initbomb(win)
    Window      win;
{
    XWindowAttributes xgwa;
    bombstruct  *bp = &bombs[screen];
    XFontStruct *c_font;
    char         number[NDIGITS+2];
 
    int         b_width, b_height;
    int         b_x, b_y;
 
    (void) XGetWindowAttributes(dsp, win, &xgwa);
    bp->width = xgwa.width;
    bp->height = xgwa.height;
    bp->x=(bp->width/2);
    bp->y=(bp->height*3/5);     /* Central-ish on the screen */
    if ( xgwa.depth == 1 )
        mono = 1;
 
                /* Set up text font */
 
    if (bp->width > 100) {      /* Full screen */
        c_font = XLoadQueryFont(dsp, FULL_COUNT_FONT);
        bp->delta = DELTA;
    }
    else {                      /* icon window */
        c_font = XLoadQueryFont(dsp, ICON_COUNT_FONT);
        bp->delta = 2;
    }
    XSetFont(dsp, Scr[screen].gc, c_font->fid);

#ifdef SIMPLE_COUNTDOWN
    (void) sprintf(number, "%0*d", NDIGITS, 0);
#else
    (void) sprintf(number, "%.*s:**", NDIGITS-2, "*******");
#endif
    bp->text_width = XTextWidth(c_font, number, NDIGITS + 1);
    bp->x -= (bp->text_width/2);
    bp->text_ascent = c_font->max_bounds.ascent;
    bp->text_descent = c_font->max_bounds.descent;
 
    if (delay > MAX_DELAY)
        delay = MAX_DELAY;              /* Time cannot move slowly */
    if ( bp->countdown == 0)            /* <--Stricter if uncommented */ 
        bp->countdown = (time(NULL) + COUNTDOWN);  /* Prime the detonator */
 
 
                /*
                 *  Draw the graphics
                 *      Very simple - detonator box with countdown
                 *
                 *  ToDo:  Improve the graphics
                 *      (e.g. stick of dynamite, with burning fuse?)
                 */
    b_width = bp->width/2;
    b_height = bp->height/3;
    b_x = bp->width/4;
    b_y = bp->height*2/5;
 
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, bp->width, bp->height);
 
    XSetForeground(dsp, Scr[screen].gc,
                allocpixel(XDefaultColormap(dsp, screen), "grey", "white"));
    XFillRectangle(dsp, win, Scr[screen].gc,
                        b_x, b_y, b_width, b_height);
 
                /*
                 *  If a full size screen (and colour),
                 *      'rivet' the box to it
                 */
    if (bp->width > 100 && !mono) {
        rivet(win, b_x + RIVET_RADIUS, b_y + RIVET_RADIUS);
        rivet(win, b_x + RIVET_RADIUS, b_y + b_height - 3*RIVET_RADIUS);
        rivet(win, b_x + b_width - 3*RIVET_RADIUS, b_y + RIVET_RADIUS);
        rivet(win, b_x + b_width - 3*RIVET_RADIUS,
                                        b_y + b_height - 3*RIVET_RADIUS);
    }
}
 
 
 
 
void
rivet( win, x, y )
    Window      win;
    int         x, y;
{
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XDrawArc(dsp, win, Scr[screen].gc, x, y, 2*RIVET_RADIUS, 2*RIVET_RADIUS,
                        70*64, 130*64);
    XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));
    XDrawArc(dsp, win, Scr[screen].gc, x, y, 2*RIVET_RADIUS, 2*RIVET_RADIUS,
                        270*64, 90*64);
}
 
 
 
 
void drawbomb(win)
    Window      win;
{
    bombstruct  *bp = &bombs[screen];
    char         number[NDIGITS+2];
    long         crayon;
    time_t       countleft;

    countleft = (bp->countdown - time(NULL)); 
    if (countleft <= 0)
        explode(win);           /* Bye, bye.... */
 
#ifdef SIMPLE_COUNTDOWN
    (void) sprintf(number, "%0*d", NDIGITS, (int) countleft);
#else
    (void) sprintf(number, "%0*d:%02d", NDIGITS-2,
      (int) countleft / 60, (int) countleft % 60);
#endif
 
                                /* Blank out the previous number .... */
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc,
                         bp->x - bp->delta,
                         (bp->y - bp->text_ascent) - bp->delta,
                         bp->text_width + (2*bp->delta),
                         (bp->text_ascent + bp->text_descent) + (2*bp->delta));
 
                                /* ... and count down */
    if ( mono )
        crayon = WhitePixel(dsp, screen);
    else
#ifdef COLOUR_CHANGE
        crayon = Scr[screen].pixels[countleft*Scr[screen].npixels/COUNTDOWN];
#else
        crayon = Scr[screen].pixels[1]);
#endif
    XSetForeground( dsp, Scr[screen].gc, crayon);
    XDrawString(dsp, win, Scr[screen].gc, bp->x, bp->y,
                number, strlen(number));
}
 
        /*
         *  Game Over - player 1
         *
         *  This user has hogged the terminal for long enough
         *  Log them out, and let someone else use it.
         */
void
explode(win)
    Window      win;
{
    bombstruct  *bp = &bombs[screen];
    char buff[NDIGITS+2];

                /*
                 *  ToDo:
                 *      Improve the graphics - some sort of explosion?
                 *      (Will need to involve the main X event loop)
                 */  
#ifdef SIMPLE_COUNTDOWN
    (void) sprintf(buff, "%.*s", NDIGITS, "*********");
#else
    (void) sprintf(buff, "%.*s:**", NDIGITS-2, "*******");
#endif
    XSetForeground( dsp, Scr[screen].gc, Scr[screen].pixels[1]);
    XDrawString(dsp, win, Scr[screen].gc, bp->x, bp->y, buff, NDIGITS);
 
#ifndef DEBUG
    if (!inwindow && !inroot && !nolock) {
        logoutUser();
        exit(-1);
    }
    else
#endif
    {
                /*
                 *  If debugging, not locked, or in a subwindow
                 *      probably inappropriate to actually log out.
                 */
        (void) fprintf(stderr, "BOOM!!!!\n");
        (void) kill(getpid(), SIGTERM);
    }
}

/* this is now in logout.c */
#if 0
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
#endif
