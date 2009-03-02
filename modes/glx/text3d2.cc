/* -*- Mode: C; tab-width: 4 -*- */
/* text3d2 --- Shows moving 3D texts */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)text3d.2cc	5.12 2004/03/09 xlockmore";

#endif

/* Copyright (c) E. Lassauge, 2004. */

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
 * This module is based on a demo of the FTGL graphics library
 * Copyright Henry Maddocks <henryj AT paradise.net.nz>
 * 	http://homepages.paradise.net.nz/henryj/
 *
 * My e-mail address is <lassauge AT users.sourceforge.net> (NOSPAM please)
 * Web site at http://lassauge.free.fr/
 *
 * Eric Lassauge  (March-09-2004)
 *
 * Revision History:
 *
 * Eric Lassauge  (March-09-2004) Created based on xscreensaver's gltext and
 * 				the FTGL library demo:
 * 				it uses freetype2 which is more common now.
 *
 */

#define DEF_TEXT        "(default)"
#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "True"
#define DEF_NOSPLIT	"False"

#ifdef STANDALONE
#define MODE_text3d2
#define PROGCLASS 	"Text3d2"
#define HACK_INIT 	init_text3d2
#define HACK_DRAW 	draw_text3d2
#define HACK_RESHAPE 	reshape_text3d2
#define text3d2_opts 	xlockmore_opts


#define DEFAULTS	"*delay:	10000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*spin:       " DEF_SPIN   "\n" \
			"*wander:     " DEF_WANDER "\n" \
			"*nosplit:    " DEF_NOSPLIT "\n" \
			"*message:    " DEF_TEXT   "\n"

extern "C"
{
  #include "xlockmore.h"	/* from the xscreensaver distribution */
}
#else				/* !STANDALONE */
  #include "xlock.h"		/* from the xlockmore distribution */
  #include "visgl.h"
  #ifdef HAS_MMOV
    #undef error
  #endif
#endif				/* !STANDALONE */
#include "iostuff.h"

#ifdef MODE_text3d2

#ifdef __CYGWIN__
  #include "FTGL/FTGLExtrdFont.h"
  #include "FTGL/FTGLOutlineFont.h"
#else
  #include "FTGLExtrdFont.h"
  #include "FTGLOutlineFont.h"
#endif

#include "rotator.h"
#include "text3d2.h"
#include <GL/glu.h>

#include <X11/Xcms.h>

/* Yes, it's an ugly mix of 'C' and 'C++' functions */
extern "C" { void init_text3d2(ModeInfo * mi); }
extern "C" { void draw_text3d2(ModeInfo * mi); }
extern "C" { void change_text3d2(ModeInfo * mi); }
extern "C" { void release_text3d2(ModeInfo * mi); }
extern "C" { void refresh_text3d2(ModeInfo * mi); }

/* arial.ttf is not supplied for legal reasons. */
/* NT and Windows 3.1 in c:\WINDOWS\SYSTEM\ARIAL.TTF */
/* Windows95 in c:\windows\fonts\arial.ttf */
#ifndef DEF_TTFONT
/* Directory of only *.ttf */
/* symbol.ttf and wingding.ttf should be excluded or it may core dump */
/* can be excluded if gltt is modified see README */
#ifdef __CYGWIN__
#define DEF_TTFONT "c:\\windows\\fonts\\"
#else
#define DEF_TTFONT "/usr/lib/X11/xlock/fonts/"
#endif
#endif


static char *mode_font;
static int nosplit;
static char *do_spin;
static Bool do_wander;
static char *fontfile = (char *) NULL;

/* Manage Option vars */

static XrmOptionDescRec opts[] =
{
    {(char *) "-ttfont", (char *) ".text3d2.ttfont", XrmoptionSepArg, (caddr_t) NULL},
    {(char *) "-no_split", (char *) ".text3d2.no_split", XrmoptionNoArg, (caddr_t) "on"},
    {(char *) "+no_split", (char *) ".text3d2.no_split", XrmoptionNoArg, (caddr_t) "off"},
    {(char *) "-wander", (char *) ".text3d2.wander", XrmoptionNoArg, (caddr_t) "on"},
    {(char *) "+wander", (char *) ".text3d2.wander", XrmoptionNoArg, (caddr_t) "off"},
    {(char *) "-spin", (char *) ".text3d2.spin", XrmoptionSepArg, (caddr_t) NULL},
    {(char *) "+spin", (char *) ".text3d2.spin", XrmoptionNoArg, (caddr_t) ""},
};

static argtype vars[] =
{
    {(void *) & mode_font, (char *) "ttfont", (char *) "TTFont", (char *) DEF_TTFONT, t_String},
    {(void *) & nosplit, (char *) "no_split", (char *) "NoSplit", (char *) DEF_NOSPLIT, t_Bool},
    {(void *) & do_wander, (char *) "wander", (char *) "Wander", (char *) DEF_WANDER, t_Bool},
    {(void *) & do_spin, (char *) "spin", (char *) "Spin", (char *) DEF_SPIN, t_String},
};

static OptionStruct desc[] =
{
    {(char *) "-ttfont filename", (char *) "Text3d2 TrueType font file name"},
    {(char *) "-/+no_split", (char *) "Text3d2 words splitting off/on"},
    {(char *) "-/+wander", (char *) "Text3d2 wander off/on"},
    {(char *) "-spin name/+spin", (char *) "Text3d2 spin mode"},
};

ModeSpecOpt text3d2_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct text3d2_description =
{"text3d2", "init_text3d2", "draw_text3d2", "release_text3d2",
 "refresh_text3d2", "change_text3d2", (char *) NULL, &text3d2_opts,
 100000, 1, 10, 1, 64, 1.0, "",
 "Shows 3D text", 0, NULL};
#endif

static text3d2struct *text3d2 = (text3d2struct *) NULL;

static GLfloat color[4] = {0.0, 0.0, 0.0, 1.0};
static GLfloat light1_ambient[4]  = { 1.0, 1.0, 1.0, 1.0 };
static GLfloat light2_ambient[4]  = { 0.2, 0.2, 0.2, 1.0 };

/*
 *-----------------------------------------------------------------------------
 *    Utils.
 *-----------------------------------------------------------------------------
 */

static void
setup_lighting()
{
   // Set up lighting.
   float light1_diffuse[4]  = { 1.0, 0.9, 0.9, 1.0 };
   float light1_specular[4] = { 1.0, 0.7, 0.7, 1.0 };
   float light1_position[4] = { -1.0, 1.0, 1.0, 0.0 };
   glLightfv(GL_LIGHT1, GL_AMBIENT,  light1_ambient);
   glLightfv(GL_LIGHT1, GL_DIFFUSE,  light1_diffuse);
   glLightfv(GL_LIGHT1, GL_SPECULAR, light1_specular);
   glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
   glEnable(GL_LIGHT1);

   float light2_diffuse[4]  = { 0.9, 0.9, 0.9, 1.0 };
   float light2_specular[4] = { 0.7, 0.7, 0.7, 1.0 };
   float light2_position[4] = { 1.0, -1.0, -1.0, 0.0 };
   glLightfv(GL_LIGHT2, GL_AMBIENT,  light2_ambient);
   glLightfv(GL_LIGHT2, GL_DIFFUSE,  light2_diffuse);
   glLightfv(GL_LIGHT2, GL_SPECULAR, light2_specular);
   glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
   glEnable(GL_LIGHT2);

   float front_emission[4] = { 0.3, 0.2, 0.1, 0.0 };
   float front_ambient[4]  = { 0.2, 0.2, 0.2, 0.0 };
   float front_diffuse[4]  = { 0.95, 0.95, 0.8, 0.0 };
   float front_specular[4] = { 0.6, 0.6, 0.6, 0.0 };
   glMaterialfv(GL_FRONT, GL_EMISSION, front_emission);
   glMaterialfv(GL_FRONT, GL_AMBIENT, front_ambient);
   glMaterialfv(GL_FRONT, GL_DIFFUSE, front_diffuse);
   glMaterialfv(GL_FRONT, GL_SPECULAR, front_specular);
   glMaterialf(GL_FRONT, GL_SHININESS, 16.0);
   glColor4fv(front_diffuse);

   glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
   glEnable(GL_CULL_FACE);
   glColorMaterial(GL_FRONT, GL_DIFFUSE);
   glEnable(GL_COLOR_MATERIAL);

   glEnable(GL_LIGHTING);
   glShadeModel(GL_SMOOTH);
}

static int
setup_font( text3d2struct * tp,
		const char* fontfile)
{
	if (!tp->wire)
		tp->font = new FTGLExtrdFont(fontfile);
	else
		tp->font = new FTGLOutlineFont(fontfile);

	if( tp->font->Error())
	{
		fprintf( stderr, "Failed to open font %s", fontfile);
		return 0;
	}

	if( !tp->font->FaceSize( 192))
	{
		fprintf( stderr, "Failed to set size");
		return(0);
	}

	tp->font->Depth(24);

	tp->font->CharMap(ft_encoding_none);

	return 1;
}

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Mode funcs.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */

void
 reshape_text3d2(ModeInfo * mi, int width, int height)
{
    text3d2struct *tp = &text3d2[MI_SCREEN(mi)];
    GLfloat h = (GLfloat) height / (GLfloat) width;

    glViewport(0, 0, tp->WinW = (GLint) width, tp->WinH = (GLint) height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective (30.0, 1/h, 1.0, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt( 0.0, 0.0, 30.0,
               0.0, 0.0, 0.0,
               0.0, 1.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

/*-------------------------------------------------------------*/
static void
gl_init (text3d2struct * tp)
{
  glEnable( GL_CULL_FACE);
  glFrontFace( GL_CCW);
  glEnable( GL_DEPTH_TEST);

  // Color is used for wire mode only
  color[0] = ((float) (NRAND(70)) / 100.0) + 0.30;
  color[1] = ((float) (NRAND(30)) / 100.0) + 0.70;
  color[2] = color[1] * 0.56;

}

/*-------------------------------------------------------------*/
static void
 draw_text(text3d2struct * tp,
      Display * display,
      Window window)
{
    int text_length;
    char *c_text;

    if (!nosplit)
    {
	text_length = index_dir(tp->words, (char *) " ");
	if (text_length == 0)
	    text_length = strlen(tp->words);
	if ((c_text = (char *) malloc(text_length+2)) != NULL)
	strncpy(c_text, tp->words, text_length);
	// +2 is because of a bug in FTGL !!
	c_text[text_length] = 0;
	c_text[text_length+1] = 0;
    }
    else
    {
	text_length = strlen(tp->words_start);
	if ((c_text = (char *) malloc(text_length+2)) != NULL)
	strncpy(c_text, tp->words_start, text_length);
	c_text[text_length] = 0;
	c_text[text_length+1] = 0;
    }

    // Render text
    tp->font->Render(c_text);

    //if (!nosplit)
	free(c_text);
}

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Xlock hooks.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 *    Initialize text3d2.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

void
 init_text3d2(ModeInfo * mi)
{
    int i;
    text3d2struct *tp;

    if (text3d2 == NULL)
    {
	if ((text3d2 = (text3d2struct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof(text3d2struct))) == NULL)
	    return;
    }
    tp = &text3d2[MI_SCREEN(mi)];
    tp->wire = MI_IS_WIREFRAME(mi);
    tp->counter = 0;

    fontfile = getModeFont(mode_font);
    if (!fontfile) {
		MI_CLEARWINDOW(mi);
		release_text3d2(mi);
		return;
    }
    if (!setup_font(tp,fontfile)) {
		(void) fprintf(stderr, "%s: unable to open True Type font %s!\n", MI_NAME(mi), fontfile);
		MI_CLEARWINDOW(mi);
		release_text3d2(mi);
		return;
    }
    if (MI_IS_DEBUG(mi)) {
		(void) fprintf(stderr,
			"%s:\n\tfontfile=%s .\n", MI_NAME(mi), fontfile);
    }

    /* Do not free fontfile getModeFont handles potential leak */
    if ((tp->glx_context = init_GL(mi)) != NULL)
    {
	if (MI_IS_USE3D(mi)) {
		// Find out the RGB values out of the left/right colors !
		XcmsColor search;
		search.pixel = MI_RIGHT_COLOR(mi);
		XcmsQueryColor(MI_DISPLAY(mi), MI_COLORMAP(mi), &search, XcmsRGBiFormat);
		light1_ambient[0] = search.spec.RGBi.red;
		light1_ambient[1] = search.spec.RGBi.green;
		light1_ambient[2] = search.spec.RGBi.blue;
		search.pixel = MI_LEFT_COLOR(mi);
		XcmsQueryColor(MI_DISPLAY(mi), MI_COLORMAP(mi), &search, XcmsRGBiFormat);
		light2_ambient[0] = search.spec.RGBi.red;
		light2_ambient[1] = search.spec.RGBi.green;
		light2_ambient[2] = search.spec.RGBi.blue;
	}
	gl_init(tp);
	reshape_text3d2(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    }
    else
    {
	MI_CLEARWINDOW(mi);
    }

   { // Manage spin
    Bool spinx=False, spiny=False, spinz=False;
    double spin_speed   = 1.0;
    double wander_speed = 0.05;
    double spin_accel   = 1.0;

    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') spinx = True;
        else if (*s == 'y' || *s == 'Y') spiny = True;
        else if (*s == 'z' || *s == 'Z') spinz = True;
        else
          {
            (void)fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     MI_NAME(mi), do_spin);
		MI_CLEARWINDOW(mi);
		release_text3d2(mi);
		return;
          }
        s++;
      }

    tp->rot = make_rotator (spinx ? spin_speed : 0,
                            spiny ? spin_speed : 0,
                            spinz ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    }

    /* Initialize displayed string */
    tp->words_start = tp->words = getWords(MI_SCREEN(mi), MI_NUM_SCREENS(mi));
    if (MI_IS_DEBUG(mi))
    {
	(void) fprintf(stderr,
		   "%s words:\n%s\n",
		   MI_NAME(mi), tp->words);
    }
}

/*
 *-----------------------------------------------------------------------------
 *    Called by the mainline code periodically to update the display.
 *-----------------------------------------------------------------------------
 */
void
 draw_text3d2(ModeInfo * mi)
{
    Display *display = MI_DISPLAY(mi);
    Window window = MI_WINDOW(mi);
    text3d2struct *tp;


    if (text3d2 == NULL) {
	  return;
	}
    tp = &text3d2[MI_SCREEN(mi)];

    if (!tp->glx_context)
	return;

    MI_IS_DRAWN(mi) = True;
    tp->counter = tp->counter + 1;

    if (tp->counter > MI_CYCLES(mi) & !nosplit)
    {
	int text_length = index_dir(tp->words, (char *) " ");

	/* Every now and then, get a new word */
	if (text_length == 0)
	    text_length = strlen(tp->words);
	tp->counter = 0;
	tp->words += text_length;
	text_length = strlen(tp->words);
	if (text_length == 0)
	{
	    tp->words_start = tp->words =
		getWords(MI_SCREEN(mi), MI_NUM_SCREENS(mi));
	}
    }
  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    double x, y, z;
    get_position (tp->rot, &x, &y, &z, 1);
    glTranslatef((x - 1.0) * 8,
                 (y - 0.5) * 8,
                 (z - 0.5) * 8);

    get_rotation (tp->rot, &x, &y, &z, 1);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }


  glScalef(0.01, 0.01, 0.01);

  if (!tp->wire)
  {
	glDisable( GL_BLEND);
    	setup_lighting();
  }
  else
  	glColor4fv (color);

  draw_text(tp, display, window);
  glPopMatrix ();

  if (MI_IS_FPS(mi)) do_fps (mi);

  glXSwapBuffers(display, window);

}

/*
 *-----------------------------------------------------------------------------
 *    The display is being taken away from us.  Free up malloc'ed
 *      memory and X resources that we've alloc'ed.  Only called
 *      once, we must zap everything for every screen.
 *-----------------------------------------------------------------------------
 */

void
 release_text3d2(ModeInfo * mi)
{
    if (text3d2 != NULL)
    {
	for (int screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
	{
	    text3d2struct *tp = &text3d2[screen];

	    if (tp->font)
		delete tp->font;
	    if (tp->rot)
		free_rotator(tp->rot);
	}
	free(text3d2);
	text3d2 = (text3d2struct *) NULL;
    }
    FreeAllGL(mi);
}

void
 refresh_text3d2(ModeInfo * mi)
{
    /* Do nothing, it will refresh by itself :) */
}

void
 change_text3d2(ModeInfo * mi)
{
    text3d2struct *tp;

    if (text3d2 == NULL) {
	  return;
	}
    tp = &text3d2[MI_SCREEN(mi)];

    if (!tp->glx_context)
	return;
    glDrawBuffer(GL_BACK);
    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(tp->glx_context));
}

#endif				/* MODE_text3d2 */
