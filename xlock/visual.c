#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)visual.c	4.07 97/01/30 xlockmore";

#endif

/*-
 * visual.c - separated out of utils.c for no good reason
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * Revision History:
 * 15-Oct-97: Separated out and modified.
 *
 */

#include "xlock.h"

extern Bool mono;
extern Bool verbose;
extern Bool debug;

static void
showVisualInfo(XVisualInfo * Vis)
{
	(void) fprintf(stderr, "Visual info: ");
	(void) fprintf(stderr, "screen %d, ", Vis->screen);
	(void) fprintf(stderr, "class %s, ", nameOfVisualClass(Vis->CLASS));
	/* (void) fprintf(stderr, "visual %ld, ", Vis->visual); */
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
		  !MI_WIN_IS_ICONIC(mi) &&
#endif
		  !MI_WIN_IS_INROOT(mi) &&
		  MI_WIN_IS_INSTALL(mi)));
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
		StaticGray, "StaticGray"
	},
	{
		GrayScale, "GrayScale"
	},
	{
		StaticColor, "StaticColor"
	},
	{
		PseudoColor, "PseudoColor"
	},
	{
		TrueColor, "TrueColor"
	},
	{
		DirectColor, "DirectColor"
	},
	{
		-1, NULL	/* Others are 0-5 respectively */
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

char       *
nameOfVisualClass(int visualclass)
{
	int         a;

	for (a = 0; VisualClassName[a].name; a++)
		if (VisualClassName[a].visualclass == visualclass)
			return (VisualClassName[a].name);
	return ("[Unknown Visual Class]");
}

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

#ifdef USE_GL

#include <GL/gl.h>
#include <GL/glx.h>

static GLXContext *glXContext = NULL;
static Bool *glOK = NULL;

/*-
 * NOTE WELL:  We _MUST_ destroy the glXContext between each mode
 * in random mode, otherwise OpenGL settings and paramaters from one
 * mode will affect the default initial state for the next mode.
 * BUT, we are going to keep the visual returned by glXChooseVisual,
 * because it will still be good (and because Mesa must keep track
 * of each one, even after XFree(), causing a small memory leak).
 */

static XVisualInfo *
getGLVisual(Display * display, int screen, XVisualInfo * wantVis, int mono)
{
	if (wantVis) {
		/* Use glXGetConfig() to see if wantVis has what we need already. */
		int         depthBits, doubleBuffer;

		/* glXGetConfig(display, wantVis, GLX_RGBA, &rgbaMode); */
		glXGetConfig(display, wantVis, GLX_DEPTH_SIZE, &depthBits);
		glXGetConfig(display, wantVis, GLX_DOUBLEBUFFER, &doubleBuffer);

		if ((depthBits > 0) && doubleBuffer) {
			return wantVis;
		} else {
			XFree((char *) wantVis);	/* Free it up since its useless now. */
			wantVis = NULL;
		}
	}
	/* If wantVis is useless, try glXChooseVisual() */
	if (mono) {
		/* Monochrome display - use color index mode */
		int         attribList[] =
		{GLX_DOUBLEBUFFER, None};

		return glXChooseVisual(display, screen, attribList);
	} else {
		int         attribList[] =
#if 1
		{GLX_RGBA, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1,
		 GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 1, None};

#else
		{GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 1, None};

#endif
		return glXChooseVisual(display, screen, attribList);
	}
}

/*-
 * The following function should be called on startup of any GL mode.
 * It returns a GLXContext for the calling mode to use with
 * glXMakeCurrent().
 */
GLXContext *
init_GL(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         screen = MI_SCREEN(mi);
	XVisualInfo xvi_in, *xvi_out;
	int         num_vis;
	GLboolean   rgbaMode;

#ifdef FX
	extern void resetSize(ModeInfo * mi, Bool setGL);

#endif

	if (!glXContext) {
		glXContext = (GLXContext *) calloc(MI_NUM_SCREENS(mi),
						   sizeof (GLXContext));
		if (!glXContext)
			return NULL;
	}
	if (glXContext[screen]) {
		glXDestroyContext(display, glXContext[screen]);
		glXContext[screen] = NULL;
	}
	xvi_in.screen = screen;
	xvi_in.visualid = XVisualIDFromVisual(MI_VISUAL(mi));
	xvi_out = XGetVisualInfo(display, VisualScreenMask | VisualIDMask,
				 &xvi_in, &num_vis);
	if (!xvi_out) {
		(void) fprintf(stderr, "Could not get XVisualInfo\n");
		return NULL;
	}
/*-
 * PURIFY 4.0.1 on SunOS4 and on Solaris 2 reports a 104 byte memory leak on
 * the next line each time that a GL mode is run in random mode when using
 * MesaGL 2.2.  This cumulative leak can cause xlock to eventually crash if
 * available memory is depleted.  This bug is fixed in MesaGL 2.3. */
	if (glOK && glOK[screen])
		glXContext[screen] = glXCreateContext(display, xvi_out, 0, GL_TRUE);

	XFree((char *) xvi_out);

	if (!glXContext[screen]) {
		if (MI_WIN_IS_VERBOSE(mi))
			(void) fprintf(stderr,
				       "GL could not create rendering context on screen %d.\n", screen);
		return (NULL);
	}
/*-
 * PURIFY 4.0.1 on Solaris2 reports an uninitialized memory read on the next
 * line. PURIFY 4.0.1 on SunOS4 does not report this error. */
	if (!glXMakeCurrent(display, window, glXContext[screen])) {
		if (MI_WIN_IS_DEBUG(mi)) {
			(void) fprintf(stderr, "GLX error\n");
			(void) fprintf(stderr, "If using MesaGL, XGetWindowAttributes is\n");
			(void) fprintf(stderr, "probably returning a null colormap in\n");
			(void) fprintf(stderr, "XMesaCreateWindowBuffer in xmesa1.c .\n");
		}
		return (NULL);
	}
	/* True Color junk */
	glGetBooleanv(GL_RGBA_MODE, &rgbaMode);
	if (!rgbaMode) {
		glIndexi(MI_WHITE_PIXEL(mi));
		glClearIndex(MI_BLACK_PIXEL(mi));
	}
#ifdef FX
	resetSize(mi, True);
#endif
	return (&(glXContext[screen]));
}

void
FreeAllGL(ModeInfo * mi)
{
	int         scr;

#ifdef FX
	extern void resetSize(ModeInfo * mi, Bool setGL);

#endif

	if (glXContext) {
		for (scr = 0; scr < MI_NUM_SCREENS(mi); scr++) {
			if (glXContext[scr]) {
				glXDestroyContext(MI_DISPLAY(mi), glXContext[scr]);
				glXContext[scr] = NULL;
			}
		}
		(void) free((void *) glXContext);
		glXContext = NULL;
	}
#ifdef FX
	resetSize(mi, False);
#endif

#if 0
	if (glOK) {
		(void) free((void *) glOK);
		glOK = NULL;
	}
#endif
}

#endif

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

void
defaultVisualInfo(Display * display, int screen)
{
	XVisualInfo *info_list = NULL, *info = NULL, vTemplate;
	int         n = 0;
	extern int  VisualClassWanted;

	vTemplate.screen = screen;
	vTemplate.depth = DisplayPlanes(display, screen);
	{
#if defined( USE_GL )		/* go with what init_GL found */
		XVisualInfo *wantVis = NULL;

		if (!glOK) {
			glOK = (Bool *) malloc(ScreenCount(display) * sizeof (Bool));
			if (!glOK)
				return;
		}
		glOK[screen] = True;
		if (VisualClassWanted == -1) {

			wantVis = NULL;		/* NULL means use the default in getVisual() */

		} else {
			vTemplate.CLASS = VisualClassWanted;
			wantVis = XGetVisualInfo(display,
			VisualScreenMask | VisualDepthMask | VisualClassMask,
						 &vTemplate, &n);

			if (n == 0) {
				/* Wanted visual not found so use default */
				wantVis = NULL;
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
