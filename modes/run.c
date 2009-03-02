/* -*- Mode: C; tab-width: 4 -*- */
/* run --- run an arbitrary mode */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)run.c	4.17 2000/04/24 xlockmore";

#endif

/*-
 * Copyright (c) 2000 by David Bagley
 *
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
 * Revision History:
 * 19-Apr-2000: Written.
 */

#ifdef STANDALONE
#define PROGCLASS "Run"
#define HACK_INIT init_run
#define HACK_DRAW draw_run
#define run_opts xlockmore_opts
#define DEFAULTS "*delay: 1000000 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_run

#ifdef VMS
/* Have to set the Logical VAXC$PATH to point to SYS$SYSTEM */
#define DEF_RUNPROGRAM "DECW$XCLOCK.EXE"
#else
#define DEF_RUNPROGRAM "xclock"
#endif
#define DEF_GEOMETRYSTRING "geometry"
/* some programs do not like "-geometry" but like "-g" */

static char *runprogram;
static char *geometrystring;

static XrmOptionDescRec opts[] =
{
	{(char *) "-runprogram", (char *) ".run.runprogram", XrmoptionSepArg, (caddr_t) NULL},
	{(char *) "-geometrystring", (char *) ".run.geometrystring", XrmoptionSepArg, (caddr_t) NULL}
};

static argtype vars[] =
{
	{(caddr_t *) & runprogram, (char *) "runprogram", (char *) "Runprogram", (char *) DEF_RUNPROGRAM, t_String},
	{(caddr_t *) & geometrystring, (char *) "geometrystring", (char *) "GeometryString", (char *) DEF_GEOMETRYSTRING, t_String}
};

static OptionStruct desc[] =
{
        {(char *) "-runprogram string", (char *) "program to run (default xclock)"},
	{(char *) "-geometrystring string", (char *) "geometry string, could be g for -g, none for none"}
};

ModeSpecOpt run_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   run_description =
{"run", "init_run", "draw_run", "release_run",
 "refresh_run", "init_run", NULL, &run_opts,
 3000000, 1, 1, 1, 64, 1.0, "",
 "Shows another xprogram", 0, NULL};

#endif

typedef struct {
      int numberprocess;
      int width, height;
} runstruct;

static runstruct *runs = NULL;

void
init_run(ModeInfo * mi)
{
	runstruct *rp;
        char geom_buf[50];
        char geoms_buf[50];
	/* char win_buf[50]; */
	XWindowAttributes xgwa;

        if (runs == NULL) {
                if ((runs = (runstruct *) calloc(MI_NUM_SCREENS(mi),
                                              sizeof (runstruct))) == NULL)
                        return;
        }
        rp = &runs[MI_SCREEN(mi)];

        if (rp->numberprocess == 0 && !MI_IS_ICONIC(mi)) {
	(void) XGetWindowAttributes(MI_DISPLAY(mi), RootWindowOfScreen(MI_SCREENPTR(mi)), &xgwa);
#ifdef DEBUG
        (void) printf("%dx%d\n", xgwa.width, xgwa.height);
#endif
        (void) sprintf(geom_buf, "%dx%d", xgwa.width, xgwa.height);
        (void) sprintf(geoms_buf, "-%s", geometrystring);

#ifdef VMS
#define FORK vfork
#else
#define FORK fork
#endif
        if ((rp->numberprocess = FORK()) == -1)
                (void) fprintf(stderr, "Fork error\n");
        else if (rp->numberprocess == 0) {
		if (strcmp(geoms_buf, "-none")) {
#ifdef DEBUG
(void) printf("running \"%s %s %s\"\n", runprogram, geoms_buf, geom_buf);
#endif
                (void) execlp(runprogram, runprogram,
                              geoms_buf, geom_buf, 0);

		} else {
#ifdef DEBUG
(void) printf("running \"%s\"\n", runprogram);
#endif

                (void) execlp(runprogram, runprogram);
        	}
        }
        }

}

/* ARGSUSED */
void
draw_run(ModeInfo * mi)
{
}

#ifdef SunCplusplus
        extern int  kill(pid_t, int);
        extern pid_t  wait(int *);

#else
#if 0
        extern int  kill(int, int);
        extern pid_t  wait(int *);

#endif
#endif

void
release_run(ModeInfo * mi)
{
	if (runs != NULL) {
                int         screen;

                for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
                        runstruct *rp = &runs[screen];

                        if (rp->numberprocess > 0) {
				int n;
#ifdef DEBUG
                        printf("killing %d\n",  rp->numberprocess);
#endif
				(void) kill(rp->numberprocess, SIGKILL);
				(void) wait(&n);
			}
                }
                (void) free((void *) runs);
                runs = NULL;
        }
}

void
refresh_run(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself :) */
}

#endif /* MODE_run */
