/* glslideshow, Copyright (c) 2003, 2004 Jamie Zawinski <jwz@jwz.org>
 * Loads a sequence of images and smoothly pans around them; crossfades
 * when loading new images.
 *
 * First version Copyright (c) 2002, 2003 Mike Oliphant (oliphant@gtk.org)
 * based on flipscreen3d, Copyright (C) 2001 Ben Buxton (bb@cactii.net).
 *
 * Almost entirely rewritten by jwz, 21-Jun-2003.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * TODO:
 *
 * - Resizing the window makes everything go black forevermore.  No idea why.
 *
 *
 * - When a new image is loaded, there is a glitch: animation pauses during
 *   the period when we're loading the image-to-fade-in.  On fast (2GHz)
 *   machines, this stutter is short but noticable (usually less than half a
 *   second.)  On slower machines, it can be much more pronounced.
 *
 *   In xscreensaver 4.17, I added the new functions fork_load_random_image()
 *   and fork_screen_to_ximage() to make it possible to do image loading in
 *   the background, in an attempt to solve this (the idea being to only swap
 *   in the new image once it has been loaded.)  Using those routines, we
 *   continue animating while the file system is being searched for an image
 *   file; while that image data is read, parsed, and decompressed; while that
 *   data is placed on a Pixmap in the X server.
 *
 *   However, two things still happen in the "parent" (glslideshow) process:
 *   converting that server-side Pixmap to a client-side XImage (XGetImage);
 *   and converting that XImage to an OpenGL texture (gluBuild2DMipmaps).
 *   It's possible that some new code would allow us to do the Pixmap-to-XImage
 *   conversion in the forked process (feed it back upstream through a pipe or
 *   SHM segment or something); however, it turns out that significant
 *   parent-process image-loading time is being spent in gluBuild2DMipmaps().
 *
 *   So, the next step would be to figure out some way to create a texture on
 *   the other end of the fork that would be usable by the parent process.  Is
 *   that even possible?  Is it possible to use a single GLX context in a
 *   multithreaded way like that?  (Or use a second GLX context, but allow the
 *   two contexts to share data?)
 *
 *   Another question remains: is the stalling happening in the GL/GLX
 *   libraries, or are we actually seeing a stall on the graphics pipeline?
 *   If the latter, then no amount of threading would help, because the
 *   bottleneck is pushing the bits from system memory to the graphics card.
 *
 *   How does Apple do this with their MacOSX slideshow screen saver?  Perhaps
 *   it's easier for them because their OpenGL libraries have thread support
 *   at a lower level?
 *
 *
 * - Even if the glitch was solved, there's still a bug in the background
 *   loading of images: as soon as the image comes in, we slap it into place
 *   in the target quad.  This can lead to an image being changed while it is
 *   still being drawn, if that quad happens to be visible already.  Instead,
 *   when the callback goes off, we should make sure to load it into the
 *   invisible quad, or if both are visible, we should wait until one goes
 *   invisible and then load it there (in other words, wait for the next
 *   fade-out to end.)
 */

#include <X11/Intrinsic.h>


# define PROGCLASS "GLSlideshow"
# define HACK_INIT init_slideshow
# define HACK_DRAW draw_slideshow
# define HACK_RESHAPE reshape_slideshow
# define HACK_HANDLE_EVENT glslideshow_handle_event
# define EVENT_MASK        (ExposureMask|VisibilityChangeMask)
# define slideshow_opts xlockmore_opts

# define DEF_FADE_DURATION  "2"
# define DEF_PAN_DURATION   "6"
# define DEF_IMAGE_DURATION "30"
# define DEF_ZOOM           "75"
# define DEF_FPS_CUTOFF     "5"
# define DEF_TITLES         "False"
# define DEF_DEBUG          "False"

#define DEFAULTS  "*delay:           20000                \n" \
                  "*fadeDuration:  " DEF_FADE_DURATION   "\n" \
                  "*panDuration:   " DEF_PAN_DURATION    "\n" \
                  "*imageDuration: " DEF_IMAGE_DURATION  "\n" \
                  "*zoom:          " DEF_ZOOM            "\n" \
                  "*FPScutoff:     " DEF_FPS_CUTOFF      "\n" \
	          "*debug   :      " DEF_DEBUG           "\n" \
		  "*wireframe:       False                \n" \
                  "*showFPS:         False                \n" \
	          "*fpsSolid:        True                 \n" \
		  "*titles:        " DEF_TITLES  "\n" \
		  "*titleFont:       -*-times-bold-r-normal-*-180-*\n" \
                  "*desktopGrabber:  xscreensaver-getimage -no-desktop %s\n"

# include "xlockmore.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifdef USE_GL

#include <GL/glu.h>
#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include "grab-ximage.h"

typedef struct {
  GLfloat x, y, w, h;
} rect;

typedef struct {
  GLuint texid;			   /* which texture to draw */
  enum { IN, OUT, DEAD } state;    /* how to draw it */
  rect from, to;		   /* the journey this quad is taking */
  char *title;
} gls_quad;


typedef struct {
  GLXContext *glx_context;
  time_t start_time;		/* when we started displaying this image */

  int motion_frames;            /* how many frames each pan takes */
  int fade_frames;              /* how many frames fading in/out takes */

  gls_quad quads[2];		/* the (up to) 2 quads we animate */
  GLuint texids[2];		/* textures: "old" and "new" */
  GLuint current_texid;         /* the "new" one */

  int img_w, img_h;		/* Size (pixels) of currently-loaded image */

  double now;			/* current time in seconds */
  double pan_start_time;	/* when this pan began */
  double image_start_time;	/* when this image was loaded */
  double dawn_of_time;		/* when the program launched */

  Bool redisplay_needed_p;	/* Sometimes we can get away with not
                                   re-painting.  Tick this if a redisplay
                                   is required. */

  GLfloat fps;                  /* approximate frame rate we're achieving */
  int pan_frame_count;		/* More frame-rate stats */
  int fade_frame_count;
  Bool low_fps_p;		/* Whether we have compensated for a low
                                   frame rate. */

  Bool fork_p;			/* threaded image loading; #### still buggy */

  XFontStruct *xfont;
  GLuint font_dlist;

} slideshow_state;

static slideshow_state *sss = NULL;


/* Command-line arguments
 */
static int fade_seconds;    /* Duration in seconds of fade transitions.
                               If 0, jump-cut instead of fading. */
static int pan_seconds;     /* Duration of each pan through an image. */
static int image_seconds;   /* How many seconds until loading a new image. */
static int zoom;            /* How far in to zoom when panning, in percent of
                               image size: that is, 75 means "when zoomed all
                               the way in, 75% of the image will be visible."
                             */
static int fps_cutoff;      /* If the frame-rate falls below this, turn off
                               zooming.*/
static Bool do_titles;	    /* Display image titles. */
static Bool debug_p;	    /* Be loud and do weird things. */


static XrmOptionDescRec opts[] = {
  {"-fade",     ".slideshow.fadeDuration",  XrmoptionSepArg, 0     },
  {"-pan",      ".slideshow.panDuration",   XrmoptionSepArg, 0     },
  {"-duration", ".slideshow.imageDuration", XrmoptionSepArg, 0     },
  {"-zoom",     ".slideshow.zoom",          XrmoptionSepArg, 0     },
  {"-cutoff",   ".slideshow.FPScutoff",     XrmoptionSepArg, 0     },
  {"-titles",   ".slideshow.titles",        XrmoptionNoArg, "True" },
  {"+titles",   ".slideshow.titles",        XrmoptionNoArg, "True" },
  {"-debug",    ".slideshow.debug",         XrmoptionNoArg, "True" },
};

static argtype vars[] = {
  { &fade_seconds,  "fadeDuration", "FadeDuration", DEF_FADE_DURATION,  t_Int},
  { &pan_seconds,   "panDuration",  "PanDuration",  DEF_PAN_DURATION,   t_Int},
  { &image_seconds, "imageDuration","ImageDuration",DEF_IMAGE_DURATION, t_Int},
  { &zoom,          "zoom",         "Zoom",         DEF_ZOOM,           t_Int},
  { &fps_cutoff,    "FPScutoff",    "FPSCutoff",    DEF_FPS_CUTOFF,     t_Int},
  { &debug_p,       "debug",        "Debug",        DEF_DEBUG,         t_Bool},
  { &do_titles,     "titles",       "Titles",       DEF_TITLES,        t_Bool},
};

ModeSpecOpt slideshow_opts = {countof(opts), opts, countof(vars), vars, NULL};


static const char *
blurb (void)
{
  static char buf[255];
  time_t now = time ((time_t *) 0);
  char *ct = (char *) ctime (&now);
  int n = strlen(progname);
  if (n > 100) n = 99;
  strncpy(buf, progname, n);
  buf[n++] = ':';
  buf[n++] = ' ';
  strncpy(buf+n, ct+11, 8);
  strcpy(buf+n+9, ": ");
  return buf;
}


/* Returns the current time in seconds as a double.
 */
static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


static void
load_font (ModeInfo *mi, char *res, XFontStruct **fontP, GLuint *dlistP)
{
  const char *font = get_string_resource (res, "Font");
  XFontStruct *f;
  Font id;
  int first, last;

  if (!font) font = "-*-times-bold-r-normal-*-180-*";

  f = XLoadQueryFont(mi->dpy, font);
  if (!f) f = XLoadQueryFont(mi->dpy, "fixed");

  id = f->fid;
  first = f->min_char_or_byte2;
  last = f->max_char_or_byte2;
  
  clear_gl_error ();
  *dlistP = glGenLists ((GLuint) last+1);
  check_gl_error ("glGenLists");
  glXUseXFont(id, first, last-first+1, *dlistP + first);
  check_gl_error ("glXUseXFont");

  *fontP = f;
}


static void
print_title_string (ModeInfo *mi, const char *string, GLfloat x, GLfloat y)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  XFontStruct *font = ss->xfont;
  GLfloat line_height = font->ascent + font->descent;

  y -= line_height;

  glPushAttrib (GL_TRANSFORM_BIT |  /* for matrix contents */
                GL_ENABLE_BIT);     /* for various glDisable calls */
  glDisable (GL_LIGHTING);
  glDisable (GL_DEPTH_TEST);
  {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    {
      glLoadIdentity();

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      {
        unsigned int i;
        int x2 = x;
        glLoadIdentity();

        gluOrtho2D (0, mi->xgwa.width, 0, mi->xgwa.height);

        glRasterPos2f (x, y);
        for (i = 0; i < strlen(string); i++)
          {
            char c = string[i];
            if (c == '\n')
              {
                glRasterPos2f (x, (y -= line_height));
                x2 = x;
              }
            else
              {
                glCallList (ss->font_dlist + (int)(c));
                x2 += (font->per_char
                       ? font->per_char[c - font->min_char_or_byte2].width
                       : font->min_bounds.width);
              }
          }
      }
      glPopMatrix();
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
  }
  glPopAttrib();

  glMatrixMode(GL_MODELVIEW);
}


static void
draw_quad (ModeInfo *mi, gls_quad *q)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat ratio;
  rect current;
  GLfloat opacity;
  double secs;
  GLfloat texw = 0;
  GLfloat texh = 0;

  if (q->state == DEAD)
    return;

  secs = ss->now - ss->pan_start_time;

  if (q->state == OUT)
    secs += pan_seconds;

  ratio = secs / (pan_seconds + fade_seconds);

  current.x = q->from.x + ratio * (q->to.x - q->from.x);
  current.y = q->from.y + ratio * (q->to.y - q->from.y);
  current.w = q->from.w + ratio * (q->to.w - q->from.w);
  current.h = q->from.h + ratio * (q->to.h - q->from.h);

  if (secs < fade_seconds)
    opacity = secs / (GLfloat) fade_seconds;    /* fading in or out... */
  else if (secs < pan_seconds)
    opacity = 1;				/* panning opaquely. */
  else
    opacity = 1 - ((secs - pan_seconds) /
                   (GLfloat) fade_seconds);    /* fading in or out... */

  if (q->state == OUT && opacity < 0.0001)
    q->state = DEAD;

  glPushMatrix();

  glTranslatef (current.x, current.y, 0);
  glScalef (current.w, current.h, 1);

  if (!wire)
    {
      texw = mi->xgwa.width  / (GLfloat) ss->img_w;
      texh = mi->xgwa.height / (GLfloat) ss->img_h;

      glEnable (GL_TEXTURE_2D);
      glEnable (GL_BLEND);
      glBindTexture (GL_TEXTURE_2D, q->texid);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask (GL_FALSE);

      /* Draw the texture quad
       */
      glColor4f (1, 1, 1, opacity);
      glNormal3f (0, 0, 1);
      glBegin (GL_QUADS);
      glTexCoord2f (0,    0);    glVertex3f (0, 0, 0);
      glTexCoord2f (0,    texh); glVertex3f (0, 1, 0);
      glTexCoord2f (texw, texh); glVertex3f (1, 1, 0);
      glTexCoord2f (texw, 0);    glVertex3f (1, 0, 0);
      glEnd();

      glDisable (GL_TEXTURE_2D);
      glDisable (GL_BLEND);
    }

  if (wire)
    glColor4f ((q->texid == ss->texids[0] ? opacity : 0), 0,
               (q->texid == ss->texids[0] ? 0 : opacity),
               opacity);
  else
    glColor4f (1, 1, 1, opacity);


  /* Draw a grid inside the box
   */
  if (wire)
    {
      GLfloat d = 0.1;
      GLfloat x, y;
      glBegin(GL_LINES);
      glVertex3f (0, 0, 0); glVertex3f (1, 1, 0);
      glVertex3f (1, 0, 0); glVertex3f (0, 1, 0);

      for (y = 0; y < 1+d; y += d)
        for (x = 0; x < 1+d; x += d)
          {
            glVertex3f (0, y, 0); glVertex3f (1, y, 0);
            glVertex3f (x, 0, 0); glVertex3f (x, 1, 0);
          }
      glEnd();
    }

  if (do_titles &&
      q->state != DEAD &&
      q->title && *q->title)
    {
      /* #### this is wrong -- I really want to draw this with
         "1,1,1,opacity", so that the text gets laid down on top
         of the image with alpha, but that doesn't work, and I
         don't know why...
       */
      glColor4f (opacity, opacity, opacity, 1);
      print_title_string (mi, q->title,
                          10, mi->xgwa.height - 10);
    }

  glPopMatrix();

  if (debug_p)
    {
      /* Draw the "from" and "to" boxes
       */
      glColor4f ((q->texid == ss->texids[0] ? opacity : 0), 0,
                 (q->texid == ss->texids[0] ? 0 : opacity),
                 opacity);

      glBegin (GL_LINE_LOOP);
      glVertex3f (q->from.x,             q->from.y,             0);
      glVertex3f (q->from.x + q->from.w, q->from.y,             0);
      glVertex3f (q->from.x + q->from.w, q->from.y + q->from.h, 0);
      glVertex3f (q->from.x,             q->from.y + q->from.h, 0);
      glEnd();

      glBegin (GL_LINE_LOOP);
      glVertex3f (q->to.x,               q->to.y,               0);
      glVertex3f (q->to.x + q->to.w,     q->to.y,               0);
      glVertex3f (q->to.x + q->to.w,     q->to.y + q->to.h,     0);
      glVertex3f (q->to.x,               q->to.y + q->to.h,     0);
      glEnd();
    }
}


static void
draw_quads (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  GLfloat s, o;
  int i;

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();

  s = (100.0 / zoom);
  o = (1-s)/2;
  glTranslatef (o, o, 0);
  glScalef (s, s, s);

  for (i = 0; i < countof(ss->quads); i++)
    draw_quad (mi, &ss->quads[i]);

  glPopMatrix();

  if (debug_p)
    {
      glColor4f (1, 1, 1, 1);
      glBegin (GL_LINE_LOOP);
      glVertex3f (0, 0, 0);
      glVertex3f (0, 1, 0);
      glVertex3f (1, 1, 0);
      glVertex3f (1, 0, 0);
      glEnd();
    }
}


/* Re-randomize the state of the given quad.
 */
static void
reset_quad (ModeInfo *mi, gls_quad *q)
{
/*  slideshow_state *ss = &sss[MI_SCREEN(mi)];*/

  GLfloat mid_w = (zoom / 100.0);
  GLfloat mid_h = (zoom / 100.0);
  GLfloat mid_x = (1 - mid_w) / 2;
  GLfloat mid_y = (1 - mid_h) / 2;

  GLfloat small = mid_w + frand ((1 - mid_w) * 0.3);
#if 0
  GLfloat large = small + frand ((1 - small) / 2) + ((1 - small) / 2);
#else
  GLfloat large = small + frand (1 - small);
#endif

  if (q->state != DEAD)
    abort();    /* we should only be resetting a quad when it's not visible. */

  /* Possible box sizes range between "zoom" and "100%".
     Pick a small box size, and a large box size.
     Assign each a random position within the 1x1 box,
     such that they encompass the middle "zoom" percentage.
     One of those is the start, and one is the end.
     Each frame will transition between one and the other.
   */

  if (random() & 1)
    {
      q->from.w = small; q->from.h = small;
      q->to.w   = large; q->to.h   = large;
    }
  else
    {
      q->from.w = large; q->from.h = large;
      q->to.w   = small; q->to.h   = small;
    }

  q->from.x = mid_x - frand (q->from.w - mid_w);
  q->from.y = mid_y - frand (q->from.h - mid_h);
  q->to.x   = mid_x - frand (q->to.w - mid_w);
  q->to.y   = mid_y - frand (q->to.w - mid_h);

  q->state = IN;
}


/* Shrinks the XImage by a factor of two.
 */
static void
shrink_image (ModeInfo *mi, XImage *ximage)
{
  int w2 = ximage->width/2;
  int h2 = ximage->height/2;
  int x, y;
  XImage *ximage2;

  if (w2 <= 32 || h2 <= 32)   /* let's not go crazy here, man. */
    return;

  if (debug_p)
    fprintf (stderr, "%s: debug: shrinking image %dx%d -> %dx%d\n",
             blurb(), ximage->width, ximage->height, w2, h2);

  ximage2 = XCreateImage (MI_DISPLAY (mi), mi->xgwa.visual,
                          32, ZPixmap, 0, 0,
                          w2, h2, 32, 0);
  ximage2->data = (char *) calloc (h2, ximage2->bytes_per_line);
  if (!ximage2->data)
    {
      fprintf (stderr, "%s: out of memory (scaling %dx%d image to %dx%d)\n",
               blurb(), ximage->width, ximage->height, w2, h2);
      exit (1);
    }
  for (y = 0; y < h2; y++)
    for (x = 0; x < w2; x++)
      XPutPixel (ximage2, x, y, XGetPixel (ximage, x*2, y*2));
  free (ximage->data);
  *ximage = *ximage2;
  ximage2->data = 0;
  XFree (ximage2);
}


/* Load a new image into a texture for the given quad.
 */
static void
load_quad_1 (ModeInfo *mi, gls_quad *q, XImage *ximage,
             const char *filename, double start_time, double cvt_time)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  int status;
  int max_reduction = 7;
  int err_count = 0;
  int wire = MI_IS_WIREFRAME(mi);
  double load_time=0, mipmap_time=0;   /* for debugging messages */

  /* if (q->state != DEAD) abort(); */

  /* Figure out which texid is currently in use, and pick the other one.
   */
  {
    GLuint tid = 0;
    int i;
    if (ss->current_texid == 0)
      tid = ss->texids[0];
    else
      for (i = 0; i < countof(ss->texids); i++)
        if (ss->texids[i] != ss->current_texid)
          {
            tid = ss->texids[i];
            break;
          }

    if (tid == 0) abort();   /* both textures in use by visible quads? */
    q->texid = tid;
    ss->current_texid = tid;
  }

  if (wire)
    goto DONE;

  if (q->title) free (q->title);
  q->title = (filename ? strdup (filename) : 0);

  if (q->title)   /* strip filename to part after last /. */
    {
      char *s = strrchr (q->title, '/');
      if (s) strcpy (q->title, s+1);
    }

  if (debug_p)
    {
      fprintf (stderr, "%s: debug: loaded    image %d: \"%s\"\n",
               blurb(), q->texid, (q->title ? q->title : "(null)"));
      load_time = double_time();
    }

  glBindTexture (GL_TEXTURE_2D, q->texid);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   GL_LINEAR_MIPMAP_LINEAR);
  
  ss->img_w = ximage->width;
  ss->img_h = ximage->height;

 AGAIN:

  clear_gl_error();
  status = gluBuild2DMipmaps (GL_TEXTURE_2D, 3,
                              ximage->width, ximage->height,
                              GL_RGBA, GL_UNSIGNED_BYTE, ximage->data);
  
  if(!status && glGetError())
   /* Some implementations of gluBuild2DMipmaps(), but set a GL error anyway.
      We could just call check_gl_error(), but that would exit. */
    status = -1;

  if (status)
    {
      char buf[100];
      const char *s = (char *) gluErrorString (status);

      if (!s || !*s)
        {
          sprintf (buf, "unknown error %d", status);
          s = buf;
        }

      clear_gl_error();

      if (++err_count > max_reduction)
        {
          fprintf(stderr,
                  "\n"
                  "%s: %dx%d texture failed, even after reducing to %dx%d:\n"
                  "%s: GLU said: \"%s\".\n"
                  "%s: probably this means "
                  "\"your video card is worthless and weak\"?\n\n",
                  blurb(), MI_WIDTH(mi), MI_HEIGHT(mi),
                  ximage->width, ximage->height,
                  blurb(), s,
                  blurb());
          exit (1);
        }
      else
        {
          if (debug_p)
            fprintf (stderr, "%s: debug: mipmap error (%dx%d): %s\n",
                     blurb(), ximage->width, ximage->height, s);
          shrink_image (mi, ximage);
          goto AGAIN;
        }
    }

  check_gl_error("mipmapping");  /* should get a return code instead of a
				    GL error, but just in case... */

  free(ximage->data);
  ximage->data = 0;
  XDestroyImage(ximage);

  if (debug_p)
    {
      fprintf (stderr, "%s: debug: mipmapped image %d: %dx%d\n",
               blurb(), q->texid, mi->xgwa.width, mi->xgwa.height);
      mipmap_time = double_time();
    }

  if (cvt_time == 0)
    cvt_time = load_time;
  if (debug_p)
    fprintf (stderr,
             "%s: debug: load time elapsed: %.2f + %.2f + %.2f = %.2f sec\n",
             blurb(),
             cvt_time    - start_time,
             load_time   - cvt_time,
             mipmap_time - load_time,
             mipmap_time - start_time);

 DONE:

  /* Re-set "now" so that time spent loading the image file does not count
     against the time remaining in this stage of the animation: image loading,
     if it takes a perceptible amount of time, will cause the animation to
     pause, but will not cause it to drop frames.
   */
  ss->now = double_time ();
  ss->image_start_time = ss->now;

  ss->redisplay_needed_p = True;
}


static void slideshow_load_cb (Screen *, Window, XImage *,
                               const char *filename, void *closure,
                               double cvt_time);

typedef struct {
  ModeInfo *mi;
  gls_quad *q;
  double start_time;
} img_load_closure;


/* Load a new image into a texture for the given quad.
 */
static void
load_quad (ModeInfo *mi, gls_quad *q)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  img_load_closure *data;

  if (debug_p)
    fprintf (stderr, "%s: debug: loading   image %d: %dx%d\n",
             blurb(), q->texid, mi->xgwa.width, mi->xgwa.height);

  if (q->state != DEAD) abort();
  if (q->title) free (q->title);
  q->title = 0;

  if (MI_IS_WIREFRAME(mi))
    return;

  data = (img_load_closure *) calloc (1, sizeof(*data));
  data->mi = mi;
  data->q = q;
  data->start_time = double_time();

  if (ss->fork_p)
    {
      fork_screen_to_ximage (mi->xgwa.screen, mi->window,
                             slideshow_load_cb, data);
    }
  else
    {
      char *title = 0;
      XImage *ximage = screen_to_ximage (mi->xgwa.screen, mi->window, &title);
      slideshow_load_cb (mi->xgwa.screen, mi->window, ximage, title, data, 0);
    }
}


static void
slideshow_load_cb (Screen *screen, Window window, XImage *ximage,
                   const char *filename, void *closure, double cvt_time)
{
  img_load_closure *data = (img_load_closure *) closure;
  load_quad_1 (data->mi, data->q, ximage, filename,
               data->start_time, cvt_time);
  memset (data, 0, sizeof (*data));
  free (data);
}


void
reshape_slideshow (ModeInfo *mi, int width, int height)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  GLfloat s;
  glViewport (0, 0, width, height);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity();

  s = 2;

  if (debug_p)
    {
      s *= (zoom / 100.0) * 0.75;
      if (s < 0.1) s = 0.1;
    }

  glScalef (s, s, s);
  glTranslatef (-0.5, -0.5, 0);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  ss->redisplay_needed_p = True;
}


Bool
glslideshow_handle_event (ModeInfo *mi, XEvent *event)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];

  if (event->xany.type == Expose ||
      event->xany.type == GraphicsExpose ||
      event->xany.type == VisibilityNotify)
    {
      if (debug_p)
        fprintf (stderr, "%s: debug: exposure\n", blurb());
      ss->redisplay_needed_p = True;
      return True;
    }
  return False;
}


/* Do some sanity checking on various user-supplied values, and make
   sure they are all internally consistent.
 */
static void
sanity_check (ModeInfo *mi)
{
  if (zoom < 1) zoom = 1;           /* zoom is a positive percentage */
  else if (zoom > 100) zoom = 100;

  if (pan_seconds < fade_seconds)   /* pan is inclusive of fade */
    pan_seconds = fade_seconds;

  if (pan_seconds == 0)             /* no zero-length cycles, please... */
    pan_seconds = 1;

  if (image_seconds < pan_seconds)  /* we only change images at fade-time */
    image_seconds = pan_seconds;

  /* If we're not panning/zooming within the image, then there's no point
     in crossfading the image with itself -- only do crossfades when changing
     to a new image. */
  if (zoom == 100 && pan_seconds < image_seconds)
    pan_seconds = image_seconds;

  if      (fps_cutoff < 0)  fps_cutoff = 0;
  else if (fps_cutoff > 30) fps_cutoff = 30;
}


/* Kludge to add "-v" to invocation of "xscreensaver-getimage" in -debug mode
 */
static void
hack_resources (void)
{
#if 0
  char *res = "desktopGrabber";
  char *val = get_string_resource (res, "DesktopGrabber");
  char buf1[255];
  char buf2[255];
  XrmValue value;
  sprintf (buf1, "%.100s.%.100s", progclass, res);
  sprintf (buf2, "%.200s -v", val);
  value.addr = buf2;
  value.size = strlen(buf2);
  XrmPutResource (&db, buf1, "String", &value);
#endif
}


void
init_slideshow (ModeInfo *mi)
{
  int screen = MI_SCREEN(mi);
  slideshow_state *ss;
  int wire = MI_IS_WIREFRAME(mi);
  int i;
  
  if (sss == NULL) {
    if ((sss = (slideshow_state *)
         calloc (MI_NUM_SCREENS(mi), sizeof(slideshow_state))) == NULL)
      return;
  }
  ss = &sss[screen];

  if ((ss->glx_context = init_GL(mi)) != NULL) {
    reshape_slideshow (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  } else {
    MI_CLEARWINDOW(mi);
  }

  if (debug_p)
    fprintf (stderr, "%s: debug: pan: %d; fade: %d; img: %d; zoom: %d%%\n",
             blurb(), pan_seconds, fade_seconds, image_seconds, zoom);

  sanity_check(mi);

  if (debug_p)
    fprintf (stderr, "%s: debug: pan: %d; fade: %d; img: %d; zoom: %d%%\n",
             blurb(), pan_seconds, fade_seconds, image_seconds, zoom);

  if (! wire)
    {
      glShadeModel (GL_SMOOTH);
      glPolygonMode (GL_FRONT_AND_BACK,GL_FILL);
      glEnable (GL_DEPTH_TEST);
      glEnable (GL_CULL_FACE);
      glCullFace (GL_FRONT);
      glDisable (GL_LIGHTING);
    }

  ss->now = double_time ();
  ss->dawn_of_time = ss->now;

  if (debug_p) glLineWidth (3);

  ss->pan_start_time   = ss->now;
  ss->image_start_time = ss->now;

  load_font (mi, "titleFont", &ss->xfont, &ss->font_dlist);

  for (i = 0; i < countof(ss->texids); i++)
    glGenTextures (1, &ss->texids[i]);
  ss->current_texid = 0;

  for (i = 0; i < countof(ss->quads); i++)
    {
      gls_quad *q = &ss->quads[i];
      q->texid = ss->current_texid;
      q->state = DEAD;
      reset_quad (mi, q);
      q->state = DEAD;
    }

  if (debug_p)
    hack_resources();

  load_quad (mi, &ss->quads[0]);
  ss->quads[0].state = IN;

  ss->redisplay_needed_p = True;

  ss->fork_p = 0; /* #### buggy */

}


/* Call this each time we change from one state to another.
   It gathers statistics on the frame rate of the previous state,
   and if it's bad, turn things off (under the assumption that
   we're running on sucky hardware.)
 */
static void
ponder_state_change (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  const char *which;
  int frames, secs;
  GLfloat fps;

  if (ss->fade_frame_count && ss->pan_frame_count)
    abort();  /* one of these should be zero! */
  else if (ss->fade_frame_count)   /* just finished fading */
    {
      which = "faded ";
      secs = fade_seconds;
      frames = ss->fade_frame_count;
      ss->fade_frame_count = 0;
    }
  else if (ss->pan_frame_count)   /* just finished panning */
    {
      which = "panned";
      secs = pan_seconds;
      frames = ss->pan_frame_count;
      ss->pan_frame_count = 0;
    }
  else
    return;  /* One of those should be non-zero! Maybe we just started,
                and the machine is insanely slow. */

  fps = frames / (GLfloat) secs;

  if (debug_p)
    fprintf (stderr, "%s: debug: %s %3d frames %2d sec %4.1f fps\n",
             blurb(), which, frames, secs, fps);


  if (fps < fps_cutoff && !ss->low_fps_p)   /* oops, this computer sucks! */
    {
      int i;

      fprintf (stderr,
               "%s: only %.1f fps!  Turning off pan/fade to compensate...\n",
               blurb(), fps);
      zoom = 100;
      fade_seconds = 0;
      ss->low_fps_p = True;

      sanity_check (mi);

      /* Reset all quads, and mark only #0 as active. */
      for (i = 0; i < countof(ss->quads); i++)
        {
          gls_quad *q = &ss->quads[i];
          q->state = DEAD;
          reset_quad (mi, q);
          q->texid = ss->current_texid;
          q->state = (i == 0 ? IN : DEAD);
        }

      ss->pan_start_time = ss->now;
      ss->redisplay_needed_p = True;

      /* Need this in case zoom changed. */
      reshape_slideshow (mi, mi->xgwa.width, mi->xgwa.height);
    }
}


void
draw_slideshow (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  double secs;

  if (!ss->glx_context)
    return;

  if (zoom < 100)
    ss->redisplay_needed_p = True;

  /* States:
      0: - A invisible,  B invisible
         - A fading in,  B invisible

      1: - A opaque,     B invisible
         - A fading out, B fading in
         - A invisible, gets reset
         - A invisible,  B opaque

      2: - A invisible,  B opaque
         - A fading in,  B fading out
         - B invisible, gets reset
         - A opaque,     B invisible (goto 1)
  */

  ss->now = double_time();

  secs = ss->now - ss->pan_start_time;

  if (secs < fade_seconds)
    {
      /* We are in the midst of a fade:
         one quad is fading in, the other is fading out.
         (If this is the very first time, then the one
         fading out is already out.)
       */
      ss->redisplay_needed_p = True;
      ss->fade_frame_count++;

      if (! ((ss->quads[0].state == IN && ss->quads[1].state == OUT) ||
             (ss->quads[1].state == IN && ss->quads[0].state == OUT) ||
             (ss->quads[0].state == IN && ss->quads[1].state == DEAD)))
        abort();
    }
  else if (secs < pan_seconds)
    {
      /* One quad is visible and in motion, the other is not.
      */
      if (ss->fade_frame_count != 0)  /* we just switched from fade to pan */
        ponder_state_change (mi);
      ss->pan_frame_count++;
    }
  else
    {
      /* One quad is visible and in motion, the other is not.
         It's time to begin fading the visible one out, and the
         invisible one in.  (Reset the invisible one first.)
       */
      gls_quad *vq, *iq;

      ponder_state_change (mi);

      if (ss->quads[0].state == IN)
        {
          vq = &ss->quads[0];
          iq = &ss->quads[1];
        }
      else
        {
          vq = &ss->quads[1];
          iq = &ss->quads[0];
        }

      if (vq->state != IN)   abort();

      /* I don't understand why sometimes iq is still OUT and not DEAD. */
      if (iq->state == OUT)  iq->state = DEAD;
      if (iq->state != DEAD) abort();

      vq->state = OUT;

      if (ss->image_start_time + image_seconds <= ss->now)
        load_quad (mi, iq);

      reset_quad (mi, iq);               /* fade invisible in */
      iq->texid = ss->current_texid;     /* make sure we're using latest img */

      ss->pan_start_time = ss->now;

      if (! ((ss->quads[0].state == IN && ss->quads[1].state == OUT) ||
             (ss->quads[1].state == IN && ss->quads[0].state == OUT)))
        abort();
    }

  ss->fps = fps_1 (mi);

  if (!ss->redisplay_needed_p)
    return;
  else if (debug_p && zoom == 100)
    fprintf (stderr, "%s: debug: drawing (%d)\n", blurb(),
             (int) (ss->now - ss->dawn_of_time));

  draw_quads (mi);
  ss->redisplay_needed_p = False;

  if (mi->fps_p) fps_2(mi);

  glFinish();
  glXSwapBuffers (MI_DISPLAY (mi), w);
}

#endif /* USE_GL */
