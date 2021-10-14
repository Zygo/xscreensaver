/* -*- Mode: C; tab-width: 4 -*- */
/* fiberlamp --- A Fiber Optic Lamp */

#if 0
static const char sccsid[] = "@(#)fiberlamp.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 2005 by Tim Auckland <tda10.geo@yahoo.com>
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
 * "fiberlamp" shows Fiber Optic Lamp.  Since there is no closed-form
 * solution to the large-amplitude cantilever equation, the flexible
 * fiber is modeled as a set of descrete nodes.
 *
 * Revision History:
 * 13-Jan-2005: Initial development.
 */

#ifdef STANDALONE
#define MODE_fiberlamp
#define DEFAULTS	"*delay: 10000  \n" \
					"*count: 500    \n" \
					"*cycles: 10000 \n" \
					"*ncolors: 64   \n" \
					"*fpsTop: true  \n" \

# define UNIFORM_COLORS
# define release_fiberlamp 0
# define reshape_fiberlamp 0
# define fiberlamp_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_fiberlamp

ENTRYPOINT ModeSpecOpt fiberlamp_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   fiberlamp_description =
{"fiberlamp", "init_fiberlamp", "draw_fiberlamp", (char *) NULL,
 "draw_fiberlamp", "change_fiberlamp", "free_fiberlamp", &fiberlamp_opts,
 1000, 500, 10000, 0, 64, 1.0, "", "Shows a Fiber Optic Lamp", 0, NULL};

#endif

#define SPREAD (30.0) /* Angular spread at the base */
#define SCALE (MI_WIDTH(mi)/2) /* Screen size */
#define NODES (20L) /* Number of nodes in a fiber.  Variable with range
					  10 .. 30, if desired.  High values have
					  stability problems unless you use small DT */

/* Physics parameters.  Tune carefully to keep realism and avoid instability*/
#define DT (0.5) /* Time increment: Low is slow, High is less stable. */
#define PY (0.12) /* Rigidity: Low droops, High is stiff. */
#define DAMPING (0.055) /* Damping: Low allows oscillations, High is boring. */

#undef PLAN /* Plan view (for debugging) */
#undef CHECKCOLORWHEEL /* Plan view with no spread */

#define DRAND(v)	(LRAND()/MAXRAND*(v))	/* double random 0 - v */

/* Length of nodes.  Uniform except for shorter notes at the tips for
   colour highlights.  Sum from 0..NODES-1 should exactly 1.0 */
#define LEN(A) ((A<NODES-3) ? 1.0/(NODES-2.5) : 0.25/(NODES-2.5))


typedef struct {
  double phi, phidash;
  double eta, etadash;
  double x;
  double y;
  double z;
} nodestruct;

typedef struct {
  nodestruct *node;
  XPoint *draw;
} fiberstruct;

typedef struct {
  int     init;
  double  psi;
  double  dpsi;
  long    count, nfibers;
  double  cx;
  double  rx, ry; /* Coordinates relative to root */
  fiberstruct *fiber;
  Bool dbufp;
  Pixmap      buffer; /* Double Buffer */
  long    bright, medium, dim; /* "White" colors */
} fiberlampstruct;

static fiberlampstruct *fiberlamps = (fiberlampstruct *) NULL;

static void
change_fiberlamp(ModeInfo * mi)
{
  fiberlampstruct *fl;
  if (fiberlamps == NULL)
	return;
  fl = &fiberlamps[MI_SCREEN(mi)];
  fl->cx = (DRAND(SCALE/4)-SCALE/8)/SCALE; /* Knock the lamp */
  fl->count = 0; /* Reset counter */
  if (fl->dbufp) {
    XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
    XFillRectangle(MI_DISPLAY(mi), fl->buffer, MI_GC(mi), 0, 0,
                   MI_WIDTH(mi), MI_HEIGHT(mi));
  }
}

static void
free_fiber(fiberlampstruct *fl)
{
	if (fl->fiber) {
		int f;

		for (f = 0; f < fl->nfibers; f++) {
			fiberstruct *fs = fl->fiber + f;

			if (fs->node)
				free(fs->node);
			if (fs->draw)
				free(fs->draw);
		}
		free(fl->fiber);
		fl->fiber = NULL;
	}
}

ENTRYPOINT void
free_fiberlamp(ModeInfo *mi)
{
    fiberlampstruct *fl = &fiberlamps[MI_SCREEN(mi)];
  	if (fl->buffer != None && fl->dbufp) {
                XFreePixmap(MI_DISPLAY(mi), fl->buffer);
                fl->buffer = None;
        }
	free_fiber(fl);
}

ENTRYPOINT void
init_fiberlamp(ModeInfo * mi)
{
  fiberlampstruct *fl;

  MI_INIT (mi, fiberlamps);
  fl = &fiberlamps[MI_SCREEN(mi)];

  /* Create or Resize double buffer */
#ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
  fl->dbufp = False;
#else
  fl->dbufp = True;
#endif

  if(fl->buffer != None && fl->buffer != MI_WINDOW(mi) && fl->dbufp)
    XFreePixmap(MI_DISPLAY(mi), fl->buffer);

  if(fl->dbufp) {
    fl->buffer = XCreatePixmap(MI_DISPLAY(mi), MI_WINDOW(mi),
                               MI_WIDTH(mi), MI_HEIGHT(mi), MI_DEPTH(mi));
    if (fl->buffer == None) {
      free_fiberlamp(mi);
      return;
    }
  } else {
    fl->buffer = MI_WINDOW(mi);
  }

  XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
  XFillRectangle(MI_DISPLAY(mi), fl->buffer, MI_GC(mi), 0, 0,
				 MI_WIDTH(mi), MI_HEIGHT(mi));

  if(fl->init) /* Nothing else to do (probably a resize) */
	return;

  fl->init = True;
  fl->nfibers = MI_COUNT(mi);
  /* Allocate fibers */
  if((fl->fiber =
	  (fiberstruct*) calloc(fl->nfibers, sizeof (fiberstruct))) == NULL) {
	free_fiberlamp(mi);
	return;
  } else {
	int f;
	for(f = 0; f < fl->nfibers; f++) {
	  fiberstruct *fs = fl->fiber + f;
	  if((fs->node =
		  (nodestruct*) calloc(NODES, sizeof (nodestruct))) == NULL
		 ||(fs->draw =
			(XPoint*) calloc(NODES, sizeof (XPoint))) == NULL) {
		free_fiberlamp(mi);
		return;
	  }
	}
  }

  {
	int f, i;
	for(f = 0; f < fl->nfibers; f++) {
	  double phi = M_PI/180 * DRAND(SPREAD);
	  double eta = DRAND(2*M_PI) - M_PI;
	  for(i = 0; i < NODES; i++) {
		nodestruct *n = &fl->fiber[f].node[i];
		n->phi = phi;
		n->phidash = 0;
		n->eta = eta;
		n->etadash = 0;
	  }
	  fl->fiber[f].node[0].etadash = 0.002/DT;
	  fl->fiber[f].node[0].y = 0;
	  fl->fiber[f].node[0].z = 0;
	}

  }

  /* Set up rotation */
  fl->psi = DRAND(2*M_PI);
  fl->dpsi = 0.01;

  /* no "NoExpose" events from XCopyArea wanted */
  XSetGraphicsExposures(MI_DISPLAY(mi), MI_GC(mi), False);

  /* Make sure we're using 'thin' lines */
  XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), 0, LineSolid, CapNotLast,
					 JoinMiter);
#ifdef CHECKCOLORWHEEL
  /* Only interested in tips, leave the rest black */
  fl->bright = fl->medium = fl->dim = MI_BLACK_PIXEL(mi);
#else
  if(MI_NPIXELS(mi) > 2) {
  	/* Set up colours for the fiber bodies.  Tips handled seperately */
	XColor c, t;
	if(XAllocNamedColor(MI_DISPLAY(mi), MI_COLORMAP(mi), "#E0E0C0", &c, &t)){
	  fl->bright = c.pixel;
	} else {
	  fl->bright = MI_WHITE_PIXEL(mi);
	}
	if(XAllocNamedColor(MI_DISPLAY(mi), MI_COLORMAP(mi), "#808070", &c, &t)){
	  fl->medium = c.pixel;
	} else {
	  fl->medium = MI_WHITE_PIXEL(mi);
	}
	if(XAllocNamedColor(MI_DISPLAY(mi), MI_COLORMAP(mi), "#404020", &c, &t)){
	  fl->dim = c.pixel;
	} else {
	  fl->dim = MI_BLACK_PIXEL(mi);
	}
  } else {
	fl->bright = MI_WHITE_PIXEL(mi);
	fl->medium = MI_WHITE_PIXEL(mi);
	fl->dim = MI_BLACK_PIXEL(mi);
  }
#endif

  /* Clear the background. */
  MI_CLEARWINDOW(mi);
  change_fiberlamp(mi);
}

/* sort fibers so they get drawn back-to-front, one bubble pass is
   enough as the order only changes slowly */
static void
sort_fibers(fiberlampstruct *fl)
{
	int i;

	for(i = 1; i < fl->nfibers; i++) {
		if (fl->fiber[i - 1].node[NODES - 1].z > fl->fiber[i].node[NODES - 1].z) {
			fiberstruct tmp = fl->fiber[i - 1];
			fl->fiber[i - 1] = fl->fiber[i];
			fl->fiber[i] = tmp;
		}
	}
}

ENTRYPOINT void
draw_fiberlamp (ModeInfo * mi)
{
  fiberlampstruct *fl;
  int f, i;
  int x, y;
  int ww, hh;
  Window unused;
  short cx, cy;

  ww = MI_WIDTH(mi);
  hh = MI_HEIGHT(mi);

  cx = MI_WIDTH(mi)/2;
#if defined PLAN || defined CHECKCOLORWHEEL
  cy = MI_HEIGHT(mi)/2;
#else
  cy = MI_HEIGHT(mi);
#endif

  if (ww > hh * 5 ||  /* window has weird aspect */
      hh > ww * 5)
    {
      if (ww > hh)
        {
          hh = ww;
          cy = hh / 4;
        }
      else
        {
          ww = hh;
          cx = 0;
          cy = hh*3/4;
        }
    }

  if (fiberlamps == NULL)
	return;
  fl = &fiberlamps[MI_SCREEN(mi)];

  fl->psi += fl->dpsi;	    /* turn colorwheel */

  XTranslateCoordinates(MI_DISPLAY(mi), MI_WINDOW(mi),
						RootWindow(MI_DISPLAY(mi),0/*#### MI_SCREEN(mi)*/),
						cx, cy, &x, &y, &unused);
  sort_fibers(fl);

  for(f = 0; f < fl->nfibers; f++) {
	fiberstruct *fs = fl->fiber + f;

	fs->node[0].eta += DT*fs->node[0].etadash;
	fs->node[0].x = fl->cx; /* Handle center movement */
	/* Handle window move.  NOTE, only x is deflected, since y doesn't
	 directly affect the physics */
	fs->node[NODES-2].x *= 0.1*(fl->ry - y);
	fs->node[NODES-2].x += 0.05*(fl->rx - x);

	/* 2nd order diff equation */
	for(i = 1; i < NODES; i++) {
	  nodestruct *n = fs->node+i;
	  nodestruct *p = fs->node+i-1;
	  double pload = 0;
	  double eload = 0;
	  double pstress = (n->phi - p->phi)*PY;
	  double estress = (n->eta - p->eta)*PY;
	  double dxi = n->x - p->x;
	  double dzi = n->z - p->z;
	  double li = sqrt(dxi*dxi + dzi*dzi)/LEN(i);
	  double drag = DAMPING*LEN(i)*LEN(i)*NODES*NODES;

	  if(li > 0) {
		int j;
		for(j = i+1; j < NODES; j++) {
		  nodestruct *nn = fs->node+j;
		  double dxj = nn->x - n->x;
		  double dzj = nn->z - n->z;

		  pload += LEN(j)*(dxi*dxj + dzi*dzj)/li; /* Radial load */
		  eload += LEN(j)*(dxi*dzj - dzi*dxj)/li; /* Transverse load */
		  /* Not a perfect simulation: in reality the transverse load
			 is only indirectly coupled to the eta deflection, but of
			 all the approaches I've tried this produces the most
			 stable model and looks the most realistic. */
		}
	  }

#ifndef CHECKCOLORWHEEL
	  n->phidash += DT*(pload - pstress - drag*n->phidash)/LEN(i);
	  n->phi += DT*n->phidash;
#endif

	  n->etadash += DT*(eload - estress - drag*n->etadash)/LEN(i);
	  n->eta += DT*n->etadash;

	  {
		double sp = sin(p->phi);
		double cp = cos(p->phi);
		double se = sin(p->eta);
		double ce = cos(p->eta);

		n->x = p->x + LEN(i-1) * ce * sp;
		n->y = p->y - LEN(i-1) * cp;
		n->z = p->z + LEN(i-1) * se * sp;
	  }

	  fs->draw[i-1].x = cx + ww/2*n->x;
#if defined PLAN || defined CHECKCOLORWHEEL /* Plan */
	  fs->draw[i-1].y = cy + ww/2*n->z;
#else /* Elevation */
	  fs->draw[i-1].y = cy + ww/2*n->y;
#endif
	}
	MI_IS_DRAWN(mi) = True;

	/* Erase: this may only be erasing an off-screen buffer, but on a
	   slow system it may still be faster than XFillRectangle() */
    /* That's unpossible. -jwz */
  }

  XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
  XFillRectangle(MI_DISPLAY(mi), fl->buffer, MI_GC(mi),
                 0, 0, MI_WIDTH(mi), MI_HEIGHT(mi));

  for(f = 0; f < fl->nfibers; f++) {
	fiberstruct *fs = fl->fiber + f;

	{
	  double x = fs->node[1].x - fl->cx + 0.025;
	  double y = fs->node[1].z + 0.02;
	  double angle = atan2(y, x) + fl->psi;
	  long tipcolor = (int)(MI_NPIXELS(mi)*angle/(2*M_PI)) % MI_NPIXELS(mi);
	  long fibercolor;
	  long tiplen;

      if (tipcolor < 0) tipcolor += MI_NPIXELS(mi);
      tipcolor = MI_PIXEL(mi, tipcolor);

	  if(fs->node[1].z < 0.0) { /* Back */
		tiplen = 2;
		fibercolor = fl->dim;
	  }else	if(fs->node[NODES-1].z < 0.7) { /* Middle */
		tiplen = 3;
		fibercolor = fl->medium;
	  } else {                 /* Front */
		tiplen = 3;
		fibercolor = fl->bright;
	  }

	  XSetForeground(MI_DISPLAY(mi), MI_GC(mi), fibercolor);
	  XDrawLines(MI_DISPLAY(mi), fl->buffer, MI_GC(mi),
				 fs->draw, NODES-tiplen, CoordModeOrigin);

	  XSetForeground(MI_DISPLAY(mi), MI_GC(mi), tipcolor);
	  XDrawLines(MI_DISPLAY(mi), fl->buffer, MI_GC(mi),
				 fs->draw+NODES-1-tiplen, tiplen, CoordModeOrigin);
	}
  }

  /* Update the screen from the double-buffer */
  if (fl->dbufp)
    XCopyArea(MI_DISPLAY(mi), fl->buffer, MI_WINDOW(mi), MI_GC(mi), 0, 0,
              MI_WIDTH(mi), MI_HEIGHT(mi), 0, 0);

  fl->rx = x;
  fl->ry = y;

  if(fl->count++ > MI_CYCLES(mi)) {
	change_fiberlamp(mi);
  }
}

#ifndef STANDALONE
ENTRYPOINT void
refresh_fiberlamp(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}
#endif

XSCREENSAVER_MODULE ("Fiberlamp", fiberlamp)

#endif /* MODE_fiberlamp */
