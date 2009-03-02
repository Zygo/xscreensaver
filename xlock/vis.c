#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)vis.c	4.07 97/01/30 xlockmore";

#endif

/*-
 * vis.c - separated out of util.c for no good reason
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * Revision History:
 * 19-Feb-01: Separated out pure GL stuff <lassauge AT users.sourceforge.net>
 * 23-Apr-97: Name conflict with xscreensaver, name changed.
 * 15-Oct-97: Separated out and modified.
 *
 */

#include "xlock.h"
#include "vis.h"

extern Bool verbose;
extern Bool debug;

static void
showVisualInfo(XVisualInfo * Vis)
{
	(void) fprintf(stderr, "Visual info: ");
	(void) fprintf(stderr, "screen %d, ", Vis->screen);
	(void) fprintf(stderr, "visual id 0x%x, ", (int) Vis->visualid);
	(void) fprintf(stderr, "class %s, ", nameOfVisualClass(Vis->CLASS));
	(void) fprintf(stderr, "depth %d\n", Vis->depth);
}

extern ScreenInfo *Scr;

Bool
fixedColors(ModeInfo * mi)
{
	Bool        temp;

#ifdef FORCEFIXEDCOLORS
	/* pretending a fixed colourmap */
	return TRUE;
#else
	/* get information about the default visual */
	temp = (!((MI_NPIXELS(mi) > 2) &&
		  (MI_VISUALCLASS(mi) != StaticGray) &&
		  (MI_VISUALCLASS(mi) != StaticColor) &&
		  (MI_VISUALCLASS(mi) != TrueColor) &&
#if 0
/*-
 * This may fix wrong colors (possibly unreadable text) in password window
 */
		  !MI_IS_ICONIC(mi) &&
#endif
		  !MI_IS_INROOT(mi) &&
		  MI_IS_INSTALL(mi)));
#endif
	if (debug) {
		(void) printf("%s colors on screen %d\n", (temp) ? "fixed" : "writeable",
			      MI_SCREEN(mi));
	}
	return temp;
}

static struct visual_class_name {
	int         visualclass;
	char       *name;
} VisualClassName[] = {

	{
		StaticGray, (char *) "StaticGray"
	},
	{
		GrayScale, (char *) "GrayScale"
	},
	{
		StaticColor, (char *) "StaticColor"
	},
	{
		PseudoColor, (char *) "PseudoColor"
	},
	{
		TrueColor, (char *) "TrueColor"
	},
	{
		DirectColor, (char *) "DirectColor"
	},
	{
		-1, (char *) NULL	/* Others are 0-5 respectively */
	},
};

int
visualClassFromName(char *name)
{
	int         a;
	char       *s1, *s2;
	int         visualclass = -1;

	for (a = 0; VisualClassName[a].name; a++) {
		for (s1 = VisualClassName[a].name, s2 = name; *s1 && *s2; s1++, s2++)
			if ((isupper((int) *s1) ? tolower((int) *s1) : *s1) !=
			    (isupper((int) *s2) ? tolower((int) *s2) : *s2))
				break;

		if ((*s1 == '\0') || (*s2 == '\0')) {

			if (visualclass != -1) {
				(void) fprintf(stderr,
					       "%s does not uniquely describe a visual class (ignored)\n", name);
				return (-1);
			}
			visualclass = VisualClassName[a].visualclass;
		}
	}
	if (visualclass == -1)
		(void) fprintf(stderr, "%s is not a visual class (ignored)\n", name);
	return (visualclass);
}

const char *
nameOfVisualClass(int visualclass)
{
	int         a;

	for (a = 0; VisualClassName[a].name; a++)
		if (VisualClassName[a].visualclass == visualclass)
			return (VisualClassName[a].name);
	return ("[Unknown Visual Class]");
}

#if 0
/*-
 * setupColormap
 *
 * Create a read/write colourmap to use
 *
 */

Bool
setupColormap(ModeInfo * mi, int *colors, Bool * truecolor,
unsigned long *red_mask, unsigned long *blue_mask, unsigned long *green_mask)
{
	/* how many colours are there altogether? */
	*colors = MI_NPIXELS(mi);
	if (*colors > MI_COLORMAP_SIZE(mi)) {
		*colors = MI_COLORMAP_SIZE(mi);
	}
	if (*colors < 2)
		*colors = 2;

	*truecolor = (MI_VISUALCLASS(mi) == TrueColor);

	*red_mask = MI_RED_MASK(mi);
	*green_mask = MI_GREEN_MASK(mi);
	*blue_mask = MI_BLUE_MASK(mi);

	return !fixedColors(mi);
}
#endif

extern Bool mono;

/*-
 * default_visual_info
 *
 * Gets a XVisualInfo structure that refers to a given visual or the default
 * visual.
 *
 * -      mi is the ModeInfo
 * -      visual is the visual to look up NULL => look up the default visual
 * -      default_info is set to point to the member of the returned list
 *          that corresponds to the default visual.
 *
 * Returns a list of XVisualInfo structures or NULL on failure. Free the list
 *   with XFree.
 */
extern int  VisualClassWanted;
#if defined( USE_GL )		/* go with what init_GL found */
 extern Bool *glOK;
#endif

void
defaultVisualInfo(Display * display, int screen)
{
	XVisualInfo *info_list = (XVisualInfo *) NULL,
		*info = (XVisualInfo *) NULL, vTemplate;
	int         n = 0;

	vTemplate.screen = screen;
	vTemplate.depth = DisplayPlanes(display, screen);
	{
#if defined( USE_GL )		/* go with what init_GL found */
		XVisualInfo *wantVis = (XVisualInfo *) NULL;

		if (!glOK) {
			glOK = (Bool *) malloc(ScreenCount(display) * sizeof (Bool));
			if (!glOK)
				return;
		}
		glOK[screen] = True;
		if (VisualClassWanted == -1) {
			/* NULL means use the default in getVisual() */
			wantVis = (XVisualInfo *) NULL;
		} else {
			vTemplate.CLASS = VisualClassWanted;
			wantVis = XGetVisualInfo(display,
			VisualScreenMask | VisualDepthMask | VisualClassMask,
						 &vTemplate, &n);

			if (n == 0) {
				/* Wanted visual not found so use default */
				wantVis = (XVisualInfo *) NULL;
			}
		}

		/* if User asked for color, try that first, then try mono */
		/* if User asked for mono.  Might fail on 16/24 bit displays,
		   so fall back on color, but keep the mono "look & feel". */
		info = info_list = getGLVisual(display, screen, wantVis, mono);
		if (!info_list) {
			info = info_list = getGLVisual(display, screen, wantVis, !mono);
			if (!info_list) {
#endif
				int         i;

				if (VisualClassWanted == -1) {
					vTemplate.CLASS = DefaultVisual(display, screen)->CLASS;
				} else {
					vTemplate.CLASS = VisualClassWanted;
				}
				info_list = XGetVisualInfo(display,
							   VisualScreenMask | VisualDepthMask | VisualClassMask,
							   &vTemplate, &n);
				if (VisualClassWanted != -1 && n == 0) {
					/* Wanted visual not found so use default */
					vTemplate.CLASS = DefaultVisual(display, screen)->CLASS;
					if (info_list)
						XFree((char *) info_list);
					info_list = XGetVisualInfo(display,
								   VisualScreenMask | VisualDepthMask | VisualClassMask,
							     &vTemplate, &n);
					if (n == 0) {
						info_list = XGetVisualInfo(display,
									   VisualScreenMask | VisualClassMask,
							     &vTemplate, &n);
					}
				}
				if ((info_list == NULL) || (n == 0)) {
					if (info_list)
						XFree((char *) info_list);
					if (verbose)
						(void) fprintf(stderr,
							       "Could get visual for screen %d.\n", screen);
					return;
				}
				if (VisualClassWanted == -1) {
					/* search through the list for the default visual */
					info = info_list;
					for (i = 0; i < n; i++, info++)
						if (info->visual == DefaultVisual(display, screen) &&
						    info->screen == screen)
							break;
				} else {
					info = info_list;
					for (i = 0; i < n; i++, info++) {
						if (info->screen == screen &&
						    info->CLASS == VisualClassWanted) {
							break;
						}
					}
					if (i == n) {
						info = info_list;
						for (i = 0; i < n; i++, info++) {
							if (info->screen == screen &&
							    info->visual == DefaultVisual(display, screen))
								break;
						}
					}
				}
#if defined( USE_GL )
				if (verbose)
					(void) fprintf(stderr, "GL can not render with root visual\n");
				glOK[screen] = False;
			}
		}
#endif
	}
	if (verbose) {
		showVisualInfo(info);
	}
	Scr[screen].depth = info->depth;
	Scr[screen].visual = info->visual;
	Scr[screen].visualclass = info->CLASS;
	Scr[screen].colormap_size = info->colormap_size;
	Scr[screen].red_mask = info->red_mask;
	Scr[screen].green_mask = info->green_mask;
	Scr[screen].blue_mask = info->blue_mask;
	XFree((char *) info_list);	/* NOT info ... its modified */
}

/* has_writeable was hacked out of xscreensaver and modified for xlockmore
 * by Jouk Jansen <joukj@hrem.stm.tudelft.nl>
 *
 * xscreensaver, Copyright (c) 1993, 1994, 1995, 1996, 1997, 1998
 *  by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */
Bool
has_writable_cells(ModeInfo * mi)
{
	switch (MI_VISUALCLASS(mi)) {
		case GrayScale:	/* Mappable grays. */
		case PseudoColor:	/* Mappable colors. */
			return True;
		case StaticGray:	/* Fixed grays. */
		case TrueColor:	/* Fixed colors. */
		case StaticColor:	/* (What's the difference again?) */
		case DirectColor:	/* DirectColor visuals are like TrueColor, but have
					   three colormaps - one for each component of RGB.
					   Screw it. */
			return False;
		default:
			abort();
	}
	return False;
}
