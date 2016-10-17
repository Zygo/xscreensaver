/* -*- Mode: C; tab-width: 4 -*- */
/* vim: set ai ts=4 sw=4: */

#if 0
/*static const char sccsid[] = "@(#)gleidescope.c	1.0 03/06/27 xlockmore";*/
#endif

/* enable -grab switch for animations */
#undef	GRAB

#undef DISPLAY_TEXTURE

/*-
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
 *	Revision History:
 *
 *	20030627	1.0		acd		First Release.
 *								Texture loading code from 'glplanet'
 *									by Jamie Zawinski <jwz@jwz.org>
 *	20030810	1.1		acd		Added size flag.
 *								Now grabs screen / video / picture
 *									(uses code from 'glslideshow' by
 *									Mike Oliphant, Ben Buxton, Jamie Zawinski).
 *								Added -duration.
 *								Added mouse code.
 *								Added fade code (also from glslideshow).
 *	20031013	1.2		acd		Migrated to compile without warnings under
 *									xscreensaver 4.13.
 *	20031023	1.3		acd		Better code to limit twisting speeds.
 *								Tweaked initial rotation values.
 *								Move, Rotate, Zoom now chosen at random if
 *									no preference is given.
 *								Made grid slightly bigger so you can't see
 *									the edge when zooming and moving.
 *	20061226	1.4		acd		Now uses GL Display Lists.
 *	20070318	1.5		acd		Generates textures.
 *								Fixed texture size problem (and introduced another).
 *	20070412	1.6		acd		Textures now have independant sizes.
 *	20070413	1.7		acd		Added Lissajous movement pattern.
 *	20070414	1.8		acd		Added corners movement pattern.
 *	20080319	1.9		acd		Changes to arguments for saner gleidescope.xml.
 *
 * TODO
 * generate textures myself - render random shapes to 256x256 texture. (done)
 * lower res for checks and random - use 256 and 4x4 or 8x8 pixels. (disabled for now)
 * gnome-saver doesn't let you specify source directory so add that to this.
 * image loading routine is too slow - rotation grinds to a halt - stop using it. (better in version 5)
 * image loading also looks bad for non-square images - edges are black. (fixed)
 * possible to see edge of the world on widescreen terminals - use larger grid and hidden hex removal?
 * fading textures may have different max_tx - use two sets. (done)
 * choice of movement patterns. (3 implemented, chooseable at compile time)
 * look into rangle and tangle.
 */

/*
**----------------------------------------------------------------------------
** Defines
**----------------------------------------------------------------------------
*/

#ifdef STANDALONE
# define DEFAULTS \
		"*delay:		20000		\n"	\
		"*showFPS:		False		\n"	\
		"*size:			0			\n"	\
		"*useSHM:		True		\n" \
		"*suppressRotationAnimation: True\n" \

# define refresh_gleidescope 0
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#include "colors.h"
#include "xpm-ximage.h"
#include "grab-ximage.h"

#ifdef GRAB
void grab_frame(Display *display, Window window);
#endif

/* acd TODO should all these be in gleidestruct? */
/* they can't be, because of the idiotic way the xlockmore "argtype vars"
   interface works. -jwz */
#ifdef GRAB
static Bool		grab;			/* grab images */
#endif
static Bool		move;			/* moving camera */
static Bool		nomove;			/* no moving camera */
static Bool		rotate;			/* rotate in place */
static Bool		norotate;		/* no rotate in place */
static Bool		zoom;			/* zooming camera */
static Bool		nozoom;			/* no zooming camera */
static char		*image;			/* name of texture to load */
static int		duration;		/* length of time to display grabbed image */

#define MAX_CAM_SPEED			1.0
#define MAX_ANGLE_VEL			1.0
#define INITIAL_ANGLE_VEL		0.2
#define INITIAL_ANGLE_ACC		0.001
#define	TWISTING_PROBABILITY	1000	/* 1 in ... of change of acceleration */

#define RADIANS	(M_PI / 180)
#define ANGLE_120	(M_PI * 2 / 3)
#define ANGLE_240	(M_PI * 4 / 3)

#define DEF_GRAB	"False"
#define DEF_MOVE	"False"
#define DEF_NOMOVE	"False"
#define DEF_ROTATE	"False"
#define DEF_NOROTATE	"False"
#define DEF_ZOOM	"False"
#define DEF_NOZOOM	"False"
#define DEF_IMAGE	"DEFAULT"
#define DEF_DURATION	"30"


static XrmOptionDescRec opts[] =
{
#ifdef GRAB
	{"-grab",		".gleidescope.grab",		XrmoptionNoArg,		"true"},
#endif
	{"-move",		".gleidescope.move",		XrmoptionNoArg,		"true"},
	{"-no-move",	".gleidescope.nomove",		XrmoptionNoArg,		"true"},
	{"-rotate",		".gleidescope.rotate",		XrmoptionNoArg,		"true"},
	{"-no-rotate",	".gleidescope.norotate",	XrmoptionNoArg,		"true"},
	{"-zoom",		".gleidescope.zoom",		XrmoptionNoArg,		"true"},
	{"-no-zoom",	".gleidescope.nozoom",		XrmoptionNoArg,		"true"},
	{"-image",		".gleidescope.image",		XrmoptionSepArg,	"DEFAULT"},
	{"-duration",	".gleidescope.duration",	XrmoptionSepArg,	"30"},
};


static argtype vars[] = {
#ifdef GRAB
	{&grab,			"grab",		"Grab",		DEF_GRAB,	t_Bool},
#endif
	{&move,			"move",		"Move",		DEF_MOVE,	t_Bool},
	{&nomove,		"nomove",	"noMove",	DEF_NOMOVE,	t_Bool},
	{&rotate,		"rotate",	"Rotate",	DEF_ROTATE,	t_Bool},
	{&norotate,		"norotate",	"noRotate",	DEF_NOROTATE,	t_Bool},
	{&zoom,			"zoom",		"Zoom",		DEF_ZOOM,	t_Bool},
	{&nozoom,		"nozoom",	"noZoom",	DEF_NOZOOM,	t_Bool},
	{&image,		"image",	"Image",	DEF_IMAGE,	t_String},
	{&duration,		"duration",	"Duration",	DEF_DURATION,		t_Int},
};

static OptionStruct desc[] = {
#ifdef GRAB
	{"-grab",		"grab images to create animation"},
#endif
	{"-move",		"camera will move"},
	{"-no-move",	"camera won't move"},
	{"-rotate",		"camera will rotate"},
	{"-no-rotate",	"camera won't rotate"},
	{"-zoom",		"camera will zoom"},
	{"-no-zoom",	"camera won't zoom"},
	{"-image",		"xpm / xbm image file to use for texture"},
	{"-duration",	"length of time texture will be used"},
};

ENTRYPOINT ModeSpecOpt gleidescope_opts = {
	sizeof opts / sizeof opts[0], opts,
	sizeof vars / sizeof vars[0], vars,
	desc
};

#ifdef USE_MODULES
ModStruct   gleidescope_description = { 
     "gleidescope", "init_gleidescope", "draw_gleidescope", "release_gleidescope",
     "draw_gleidescope", "init_gleidescope", NULL, &gleidescope_opts,
     1000, 1, 2, 1, 4, 1.0, "",
     "GL Kaleidescope", 0, NULL};
#endif

/*
**-----------------------------------------------------------------------------
**	Typedefs
**-----------------------------------------------------------------------------
*/

typedef struct hex_s {
	GLfloat	x, y, z;		/* position */
} hex_t;

typedef struct {
	GLfloat x;
	GLfloat y;
	GLfloat z;
} vectorf;

typedef struct {
	GLfloat x;
	GLfloat y;
} vector2f;

typedef struct {
	GLuint			id;				/* opengl texture id */
	GLfloat			width, height;	/* texture width and height */
	GLfloat			min_tx, min_ty;	/* minimum texture sizes */
	GLfloat			max_tx, max_ty;	/* maximum texture sizes */
	time_t			start_time;
	Bool			button_down_p;
    Bool			mipmap_p;
    Bool			waiting_for_image_p;
	/* r_phase is for triangle rotation speed */
	GLfloat			x_period, y_period, r_period;
	GLfloat			x_phase, y_phase, r_phase;
} texture;

#define	MAX_FADE	500	/* number of fade cycles */

typedef struct {
	float			cam_x_speed, cam_z_speed, cam_y_speed;
	int				cam_x_phase, cam_z_phase, cam_y_phase;
	float			tic;
	GLXContext		*glx_context;
	Window			window;
	texture			textures[2];	/* texture handles */
	GLuint			visible;		/* index for current texture */
	GLint			fade;
	time_t			start_time;
	Bool			button_down_p;

    int		size;
	int		list;

    float	tangle;		/* texture angle (degrees) */
    float	tangle_vel;	/* texture velocity */
    float	tangle_acc;	/* texture acceleration */

    float	rangle;		/* rotate angle */
    float	rangle_vel;	/* rotate velocity */
    float	rangle_acc;	/* rotate acceleration */

    /* mouse */
    int xstart;
    int ystart;
    double xmouse;
    double ymouse;

    Bool mipmap_p;
    Bool waiting_for_image_p;

} gleidestruct;

#define frandrange(x, y)	(x + frand(y - x))

#define	XOFFSET	(0.8660254f)	/* sin 60' */
#define	YOFFSET	(1.5000000f)	/* cos 60' + 1 */

#if 0

#define	SIZE	3

/* generates a grid with edges of given size */
/* acd TODO - replace hex[] with this and allow size and distance as parameters */

int
generate_grid(int size)

	int	i, x, y;

	gp->size--;

	i = gp->size;
	for (y = -size ; y <= size ; y++) {
		for (x = -i ; x <= i ; x += 2) {
			printf("{XOFFSET * %d, YOFFSET * %d, 0},\n", x, y);
		}
		printf("\n");
		if (y < 0) {
			i++;
		} else {
			i--;
		}
	}
	return 0;
}
#endif

/* acd - this is terrible - 120+ hexes */
static const hex_t hex[] = {
	/* edges of size 7 */
	/* number of hexagons required to cover screen depends on camera distance */
	/* at a distance of 10 this is just about enough. */
	{XOFFSET * -6, YOFFSET * -6, 0},
	{XOFFSET * -4, YOFFSET * -6, 0},
	{XOFFSET * -2, YOFFSET * -6, 0},
	{XOFFSET * 0, YOFFSET * -6, 0},
	{XOFFSET * 2, YOFFSET * -6, 0},
	{XOFFSET * 4, YOFFSET * -6, 0},
	{XOFFSET * 6, YOFFSET * -6, 0},

	{XOFFSET * -7, YOFFSET * -5, 0},
	{XOFFSET * -5, YOFFSET * -5, 0},
	{XOFFSET * -3, YOFFSET * -5, 0},
	{XOFFSET * -1, YOFFSET * -5, 0},
	{XOFFSET * 1, YOFFSET * -5, 0},
	{XOFFSET * 3, YOFFSET * -5, 0},
	{XOFFSET * 5, YOFFSET * -5, 0},
	{XOFFSET * 7, YOFFSET * -5, 0},

	{XOFFSET * -8, YOFFSET * -4, 0},
	{XOFFSET * -6, YOFFSET * -4, 0},
	{XOFFSET * -4, YOFFSET * -4, 0},
	{XOFFSET * -2, YOFFSET * -4, 0},
	{XOFFSET * 0, YOFFSET * -4, 0},
	{XOFFSET * 2, YOFFSET * -4, 0},
	{XOFFSET * 4, YOFFSET * -4, 0},
	{XOFFSET * 6, YOFFSET * -4, 0},
	{XOFFSET * 8, YOFFSET * -4, 0},

	{XOFFSET * -9, YOFFSET * -3, 0},
	{XOFFSET * -7, YOFFSET * -3, 0},
	{XOFFSET * -5, YOFFSET * -3, 0},
	{XOFFSET * -3, YOFFSET * -3, 0},
	{XOFFSET * -1, YOFFSET * -3, 0},
	{XOFFSET * 1, YOFFSET * -3, 0},
	{XOFFSET * 3, YOFFSET * -3, 0},
	{XOFFSET * 5, YOFFSET * -3, 0},
	{XOFFSET * 7, YOFFSET * -3, 0},
	{XOFFSET * 9, YOFFSET * -3, 0},

	{XOFFSET * -10, YOFFSET * -2, 0},
	{XOFFSET * -8, YOFFSET * -2, 0},
	{XOFFSET * -6, YOFFSET * -2, 0},
	{XOFFSET * -4, YOFFSET * -2, 0},
	{XOFFSET * -2, YOFFSET * -2, 0},
	{XOFFSET * 0, YOFFSET * -2, 0},
	{XOFFSET * 2, YOFFSET * -2, 0},
	{XOFFSET * 4, YOFFSET * -2, 0},
	{XOFFSET * 6, YOFFSET * -2, 0},
	{XOFFSET * 8, YOFFSET * -2, 0},
	{XOFFSET * 10, YOFFSET * -2, 0},

	{XOFFSET * -11, YOFFSET * -1, 0},
	{XOFFSET * -9, YOFFSET * -1, 0},
	{XOFFSET * -7, YOFFSET * -1, 0},
	{XOFFSET * -5, YOFFSET * -1, 0},
	{XOFFSET * -3, YOFFSET * -1, 0},
	{XOFFSET * -1, YOFFSET * -1, 0},
	{XOFFSET * 1, YOFFSET * -1, 0},
	{XOFFSET * 3, YOFFSET * -1, 0},
	{XOFFSET * 5, YOFFSET * -1, 0},
	{XOFFSET * 7, YOFFSET * -1, 0},
	{XOFFSET * 9, YOFFSET * -1, 0},
	{XOFFSET * 11, YOFFSET * -1, 0},

	{XOFFSET * -12, YOFFSET * 0, 0},
	{XOFFSET * -10, YOFFSET * 0, 0},
	{XOFFSET * -8, YOFFSET * 0, 0},
	{XOFFSET * -6, YOFFSET * 0, 0},
	{XOFFSET * -4, YOFFSET * 0, 0},
	{XOFFSET * -2, YOFFSET * 0, 0},
	{XOFFSET * 0, YOFFSET * 0, 0},
	{XOFFSET * 2, YOFFSET * 0, 0},
	{XOFFSET * 4, YOFFSET * 0, 0},
	{XOFFSET * 6, YOFFSET * 0, 0},
	{XOFFSET * 8, YOFFSET * 0, 0},
	{XOFFSET * 10, YOFFSET * 0, 0},
	{XOFFSET * 12, YOFFSET * 0, 0},

	{XOFFSET * -11, YOFFSET * 1, 0},
	{XOFFSET * -9, YOFFSET * 1, 0},
	{XOFFSET * -7, YOFFSET * 1, 0},
	{XOFFSET * -5, YOFFSET * 1, 0},
	{XOFFSET * -3, YOFFSET * 1, 0},
	{XOFFSET * -1, YOFFSET * 1, 0},
	{XOFFSET * 1, YOFFSET * 1, 0},
	{XOFFSET * 3, YOFFSET * 1, 0},
	{XOFFSET * 5, YOFFSET * 1, 0},
	{XOFFSET * 7, YOFFSET * 1, 0},
	{XOFFSET * 9, YOFFSET * 1, 0},
	{XOFFSET * 11, YOFFSET * 1, 0},

	{XOFFSET * -10, YOFFSET * 2, 0},
	{XOFFSET * -8, YOFFSET * 2, 0},
	{XOFFSET * -6, YOFFSET * 2, 0},
	{XOFFSET * -4, YOFFSET * 2, 0},
	{XOFFSET * -2, YOFFSET * 2, 0},
	{XOFFSET * 0, YOFFSET * 2, 0},
	{XOFFSET * 2, YOFFSET * 2, 0},
	{XOFFSET * 4, YOFFSET * 2, 0},
	{XOFFSET * 6, YOFFSET * 2, 0},
	{XOFFSET * 8, YOFFSET * 2, 0},
	{XOFFSET * 10, YOFFSET * 2, 0},

	{XOFFSET * -9, YOFFSET * 3, 0},
	{XOFFSET * -7, YOFFSET * 3, 0},
	{XOFFSET * -5, YOFFSET * 3, 0},
	{XOFFSET * -3, YOFFSET * 3, 0},
	{XOFFSET * -1, YOFFSET * 3, 0},
	{XOFFSET * 1, YOFFSET * 3, 0},
	{XOFFSET * 3, YOFFSET * 3, 0},
	{XOFFSET * 5, YOFFSET * 3, 0},
	{XOFFSET * 7, YOFFSET * 3, 0},
	{XOFFSET * 9, YOFFSET * 3, 0},

	{XOFFSET * -8, YOFFSET * 4, 0},
	{XOFFSET * -6, YOFFSET * 4, 0},
	{XOFFSET * -4, YOFFSET * 4, 0},
	{XOFFSET * -2, YOFFSET * 4, 0},
	{XOFFSET * 0, YOFFSET * 4, 0},
	{XOFFSET * 2, YOFFSET * 4, 0},
	{XOFFSET * 4, YOFFSET * 4, 0},
	{XOFFSET * 6, YOFFSET * 4, 0},
	{XOFFSET * 8, YOFFSET * 4, 0},

	{XOFFSET * -7, YOFFSET * 5, 0},
	{XOFFSET * -5, YOFFSET * 5, 0},
	{XOFFSET * -3, YOFFSET * 5, 0},
	{XOFFSET * -1, YOFFSET * 5, 0},
	{XOFFSET * 1, YOFFSET * 5, 0},
	{XOFFSET * 3, YOFFSET * 5, 0},
	{XOFFSET * 5, YOFFSET * 5, 0},
	{XOFFSET * 7, YOFFSET * 5, 0},

	{XOFFSET * -6, YOFFSET * 6, 0},
	{XOFFSET * -4, YOFFSET * 6, 0},
	{XOFFSET * -2, YOFFSET * 6, 0},
	{XOFFSET * 0, YOFFSET * 6, 0},
	{XOFFSET * 2, YOFFSET * 6, 0},
	{XOFFSET * 4, YOFFSET * 6, 0},
	{XOFFSET * 6, YOFFSET * 6, 0},
};

/*
**----------------------------------------------------------------------------
** Local Variables
**----------------------------------------------------------------------------
*/

static	gleidestruct *gleidescope = NULL;

#if 0
/*
 *load defaults in config structure
 */
static void setdefaultconfig(void)
{
#ifdef GRAB
	grab = False;
#endif
	move = False;
	rotate = False;
	zoom = False;
	image = NULL;
}
#endif

ENTRYPOINT Bool
gleidescope_handle_event(ModeInfo *mi, XEvent *event)
{
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];

	/*
	printf("event:%d\n", event->xany.type);
	printf("button:%d\n", event->xbutton.button);
	*/
	if (event->xany.type == ButtonPress)
      {
			if (event->xbutton.button == Button1 ||
                event->xbutton.button == Button3)
			{
				/* store initial values of mouse */
				gp->xstart = event->xbutton.x;
				gp->ystart = event->xbutton.y;

				/* button is down */
				gp->button_down_p = True;
				return True;
			}
#if 0	/* TODO */
			else if (event->xbutton.button == Button4)
			{
				/* zoom in */
				return True;
			}
			else if (event->xbutton.button == Button5)
			{
				/* zoom out */
				return True;
			}
#endif
            } else if (event->xany.type == ButtonRelease)
      {
			if (event->xbutton.button == Button1 ||
                event->xbutton.button == Button3)
			{
				/* button is up */
				gp->button_down_p = False;
				return True;
			}
            } else if (event->xany.type == MotionNotify)
      {
			if (gp->button_down_p)
			{
				/* update mouse position */
				gp->xmouse += (double)(event->xmotion.x - gp->xstart) / MI_WIDTH(mi);
				gp->ymouse += (double)(event->xmotion.y - gp->ystart) / MI_HEIGHT(mi);
				gp->xstart = event->xmotion.x;
				gp->ystart = event->xmotion.y;

				return True;
			}
      }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      gp->start_time = -1;
      gp->fade = 0;
      return True;
    }

	return False;
}


static void
image_loaded_cb (const char *filename, XRectangle *geometry,
                 int image_width, int image_height, 
                 int texture_width, int texture_height,
                 void *closure)
{
	texture *tp = (texture *) closure;

#if 0
	gp->max_tx = (GLfloat) image_width  / texture_width;
	gp->max_ty = (GLfloat) image_height / texture_height;
#endif

	/* new - taken from flipscreen */
	tp->width = texture_width;
	tp->height = texture_height;
	tp->min_tx = (GLfloat) geometry->x / tp->width;
	tp->min_ty = (GLfloat) geometry->y / tp->height;
	tp->max_tx = (GLfloat) (geometry->x + geometry->width)  / tp->width;
	tp->max_ty = (GLfloat) (geometry->y + geometry->height) / tp->height;

#ifdef DEBUG
	printf("Image w,h: (%d, %d)\n", image_width, image_height);
	printf("Texture w,h: (%d, %d)\n", texture_width, texture_height);
	printf("Geom x,y: (%d, %d)\n", geometry->x, geometry->y);
	printf("Geom w,h: (%d, %d)\n", geometry->width, geometry->height);
	printf("Max Tx,Ty: (%f, %f)\n", tp->max_tx, tp->max_ty);
#endif

	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			(tp->mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));

	tp->waiting_for_image_p = False;
	tp->start_time = time ((time_t *) 0);
}

static void
getSnapshot(ModeInfo *mi, texture *texture)
{
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];

#ifdef DEBUG
	printf("getSnapshot");
#endif

	if (MI_IS_WIREFRAME(mi))
		return;

    gp->mipmap_p = True;
    load_texture_async (mi->xgwa.screen, mi->window,
                        *gp->glx_context, 0, 0, gp->mipmap_p, 
                        texture->id, image_loaded_cb, texture);
	texture->start_time = time((time_t *)0);
}

#define TEXTURE_SIZE	256

static void
plot(unsigned char *buffer, int x, int y, int r, int g, int b, int a) {
	int c;
	if (x < 0 || x >= TEXTURE_SIZE || y < 0 || y >= TEXTURE_SIZE) {
		return;
	}
	c = ((x * TEXTURE_SIZE) + y) * 4;
	/*printf("(%d,%d)[%d]\n", x, y, c);*/
	buffer[c + 0] = r;
	buffer[c + 1] = g;
	buffer[c + 2] = b;
	buffer[c + 3] = a;
}

#if 0
static void
plot2(unsigned char *buffer, int x, int y, int r, int g, int b, int a) {
	int c;
	if (x < 0 || x >= TEXTURE_SIZE || y < 0 || y >= TEXTURE_SIZE) {
		return;
	}
	c = ((x * TEXTURE_SIZE) + y) * 4;
	/*printf("(%d,%d)[%d]\n", x, y, c);*/
	buffer[c + 0] = r;
	buffer[c + 1] = g;
	buffer[c + 2] = b;
	buffer[c + 3] = a;
	
	if (y + 1 < TEXTURE_SIZE) {
		buffer[c + 4] = r;
		buffer[c + 5] = g;
		buffer[c + 6] = b;
		buffer[c + 7] = a;
	}

	if (x + 1 < TEXTURE_SIZE) {
		c += (TEXTURE_SIZE * 4);
		buffer[c + 0] = r;
		buffer[c + 1] = g;
		buffer[c + 2] = b;
		buffer[c + 3] = a;
		if (y + 1 < TEXTURE_SIZE) {
			buffer[c + 4] = r;
			buffer[c + 5] = g;
			buffer[c + 6] = b;
			buffer[c + 7] = a;
		}
	}
}
#endif

/* draw geometric shapes to texture */
/* modifies passed in buffer */
static void
draw_shapes (unsigned char *buffer) {
	int a = 0xff;
	int x, y, w, h;
	int i, j;
	int s;
	float left, right;

	for (i = 0 ; i < TEXTURE_SIZE * TEXTURE_SIZE * 4 ; i += 4) {
		buffer[i + 0] = 0x00;
		buffer[i + 1] = 0x00;
		buffer[i + 2] = 0x00;
		buffer[i + 3] = 0xff;
	}

	for (s = 0 ; s < 25 ; s++) {
		int shape = random() % 3;

		/* 8 bits */
		int r = (random() & 0xff);
		int g = (random() & 0xff);
		int b = (random() & 0xff);

		switch (shape) {
			case 0:
				/* rectangle */
				x = (random() % TEXTURE_SIZE) - (TEXTURE_SIZE / 4);	/* top left */
				y = (random() % TEXTURE_SIZE) - (TEXTURE_SIZE / 4);
				w = 10 + random() % (TEXTURE_SIZE / 4);	/* size */
				h = 10 + random() % (TEXTURE_SIZE / 4);
#ifdef DEBUG
				printf("Rectangle: (%d, %d)(%d, %d)\n", x, y, w, h);
#endif
				if (x < 0) {
					x = 0;
				}
				if (y < 0) {
					y = 0;
				}
				for (i = x ; i < x + w && i < TEXTURE_SIZE; i++) {
					for (j = y ; j < y + h && j < TEXTURE_SIZE; j++) {
						plot(buffer, i, j, r, g, b, a);
					}
				}
				break;

			case 1:
				/* circle */
				x = random() % TEXTURE_SIZE;	/* centre */
				y = random() % TEXTURE_SIZE;
				h = 10 + random() % (TEXTURE_SIZE / 8);	/* radius */
#ifdef DEBUG
				printf("Circle: %d, %d, %d\n", x, y, h);
#endif
				for (i = 0 ; i < h ; i++) {
					int xdist = i * i;
					for (j = 0 ; j < h ; j++) {
						int ydist = j * j;
						/*
						printf("xdist: %d\n", xdist);
						printf("ydist: %d\n", ydist);
						printf("radius: %d\n", h * h);
						*/
						if ((xdist + ydist) < (h * h)) {
							plot(buffer, x + i, y + j, r, b, g, a);
							/* check we haven't already done these */
							if (j != 0) {
								plot(buffer, x + i, y - j, r, b, g, a);
							}
							if (i != 0) {
								plot(buffer, x - i, y + j, r, b, g, a);
								if (j != 0) {
								plot(buffer, x - i, y - j, r, b, g, a);
								}
							}
						}
					}
				}
				break;

			case 2:
				/* triangle */
				x = random() % TEXTURE_SIZE;	/* top */
				y = random() % TEXTURE_SIZE;
				h = 10 + random() % (TEXTURE_SIZE / 4);	/* height */
#ifdef DEBUG
				printf("Triangle: %d, %d, %d\n", x, y, h);
#endif
				left = x;
				right = x;
				for (i = 0 ; i < h ; i++) {
					for (j = left ; j < right ; j++) {
						plot(buffer, j, y + i, r, g, b, a);
					}
					left -= .5;
					right += .5;
				}
				break;
		}
	}
}

static void
setup_random_texture (ModeInfo *mi, texture *texture)
{
	int width = 0, height = 0;
	char buf[1024];
	unsigned char *my_data = NULL;
#if 0
	int i, j, c;
	int style;
	int r0, g0, b0, a0, r1, g1, b1, a1;
#endif

#ifdef DEBUG
	printf("RandomTexture\n");
#endif

	/* use this texture */
	glBindTexture(GL_TEXTURE_2D, texture->id);

	clear_gl_error();

	/*
	 * code for various generated patterns - noise, stripes, checks etc.
	 * random geometric shapes looked the best.
	 */

#if 0
	style = random() & 0x3;
	r0 = random() & 0xff; 
	g0 = random() & 0xff; 
	b0 = random() & 0xff; 
	a0 = 0xff;
	r1 = random() & 0xff; 
	g1 = random() & 0xff; 
	b1 = random() & 0xff; 
	a1 = 0xff;

	switch (style) {
		case 0:	/* random */
			printf("Random0\n");
			height = width = TEXTURE_SIZE;
			my_data = (void *)malloc(width * height * 4);
			for (i = 0 ; i < width ; i += 2) {
				for (j = 0 ; j < height ; j += 2) {
					r0 = random() & 0xff; 
					g0 = random() & 0xff; 
					b0 = random() & 0xff; 
					a0 = 0xff;
					plot2(my_data, i, j, r0, g0, b0, a0);
				}
			}
			break;

		case 1:	/* shapes */
#endif
#ifdef DEBUG
			printf("Shapes\n");
#endif
			height = width = TEXTURE_SIZE;
			my_data = (void *)malloc(width * height * 4);
			draw_shapes(my_data);
#if 0
			break;

		case 2:	/* check */
			printf("Check\n");
			height = width = TEXTURE_SIZE;
			my_data = (void *)malloc(width * height * 4);
			for (i = 0 ; i < height ; i += 2) {
				for (j = 0 ; j < width ; j += 2) {
					if (((i + j) & 0x3) == 0) {
						plot2(my_data, i, j, r0, g0, b0, a0);
					} else {
						plot2(my_data, i, j, r1, g1, b1, a1);
					}
				}
			}
			break;

		case 3:	/* random stripes */
			printf("Stripes 2\n");
			height = width = TEXTURE_SIZE;
			my_data = (void *)malloc(width * height * 4);
			for (i = 0 ; i < height ; i += 2) {
				r0 = random() & 0xff; 
				g0 = random() & 0xff; 
				b0 = random() & 0xff; 
				a0 = 0xff;
				for (j = 0 ; j < width ; j += 2) {
					plot2(my_data, i, j, r0, g0, b0, a0);
				}
			}
			break;
	}
#endif

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
			width, height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, my_data);
	sprintf (buf, "random texture: (%dx%d)",
			width, height);
	check_gl_error(buf);

	/* setup parameters for texturing */
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, width);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	if (random() & 0x1) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	if (my_data != NULL) {
		free(my_data);
		my_data = NULL;
	}

	/* use full texture */
	/* acd - was 1.0 */
	texture->min_tx = 0.0;
	texture->max_tx = 2.0;
	texture->min_ty = 0.0;
	texture->max_ty = 2.0;
	texture->start_time = time((time_t *)0);
}

static void
setup_file_texture (ModeInfo *mi, char *filename, texture *texture)
{
	Display *dpy = mi->dpy;
	Visual *visual = mi->xgwa.visual;
	char buf[1024];

	Colormap cmap = mi->xgwa.colormap;
	XImage *image = xpm_file_to_ximage (dpy, visual, cmap, filename);

#ifdef DEBUG
	printf("FileTexture\n");
#endif

	/* use this texture */
	glBindTexture(GL_TEXTURE_2D, texture->id);

	clear_gl_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
			image->width, image->height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, image->data);
	sprintf (buf, "texture: %.100s (%dx%d)",
			filename, image->width, image->height);
	check_gl_error(buf);

	/* setup parameters for texturing */
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, image->width);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	/* use full texture */
	texture->min_tx = 0.0;
	texture->max_tx = 1.0;
	texture->min_ty = 0.0;
	texture->max_ty = 1.0;
	texture->start_time = time((time_t *)0);
}

static void
setup_texture(ModeInfo * mi, texture *texture)
{
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];

	if (!image || !*image || !strcmp(image, "DEFAULT")) {
		/* no image specified - use system settings */
#ifdef DEBUG
		printf("SetupTexture: get_snapshot\n");
#endif
		getSnapshot(mi, texture);
	} else {
		if (strcmp(image, "GENERATE") == 0) {
#ifdef DEBUG
			printf("SetupTexture: random_texture\n");
#endif
			setup_random_texture(mi, texture);
		} else {
			/* use supplied image file */
#ifdef DEBUG
			printf("SetupTexture: file_texture\n");
#endif
			setup_file_texture(mi, image, texture);
		}
	}
	/* copy start time from texture */
	gp->start_time = texture->start_time;

	check_gl_error("texture initialization");

	/* acd 
	 * resultant loaded image is upside down BUT
	 * it's a kaledescope and half of the hexagon is backwards anyway...
	 */

	/* TODO: values for lissajous movement */
	texture->x_period = frandrange(-2.0, 2.0);
	texture->y_period = frandrange(-2.0, 2.0);
	texture->r_period = frandrange(-2.0, 2.0);
	texture->x_phase = frand(M_PI * 2);
	texture->y_phase = frand(M_PI * 2);
	texture->r_phase = frand(M_PI * 2);
#ifdef DEBUG
	printf("XPeriod %f XPhase %f\n", texture->x_period, texture->x_phase);
	printf("YPeriod %f YPhase %f\n", texture->y_period, texture->y_phase);
	printf("RPeriod %f RPhase %f\n", texture->r_period, texture->r_phase);
#endif
}

#define	VERTEX0	glVertex3f( 0.0000f,  0.000f, 0.0f);
#define	VERTEX1	glVertex3f( 0.0000f,  1.000f, 0.0f);
#define	VERTEX2	glVertex3f( XOFFSET,  0.500f, 0.0f);
#define	VERTEX3	glVertex3f( XOFFSET, -0.500f, 0.0f);
#define	VERTEX4	glVertex3f( 0.0000f, -1.000f, 0.0f);
#define	VERTEX5	glVertex3f(-XOFFSET, -0.500f, 0.0f);
#define	VERTEX6	glVertex3f(-XOFFSET,  0.500f, 0.0f);

/*
** Three different functions for calculating texture coordinates
** which modify how the texture triangle moves over the source image.
** Choose one.
*/

#if 0
/* the classic equilateral triangle rotating around centre */
static void
calculate_texture_coords(ModeInfo *mi, texture *texture, vector2f t[3]) {

	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];
	GLfloat	centre_x = 0.5;
	GLfloat	centre_y = 0.5;
	GLfloat radius_x = (texture->max_tx - texture->min_tx) / 2;
	GLfloat radius_y = (texture->max_ty - texture->min_ty) / 2;
	GLfloat tangle2;
	t[0].x = centre_x;
	t[0].y = centre_y;

	/* t[1] */
	t[1].x = centre_x + .95 * radius_x * cos((gp->ymouse * 2 * M_PI) + (gp->tangle * RADIANS));
	t[1].y = centre_y + .95 * radius_y * sin((gp->ymouse * 2 * M_PI) + (gp->tangle * RADIANS));

	/* t[2] is always 60' further around than t2 */
	tangle2 = (gp->ymouse * 2 * M_PI) + (gp->tangle * RADIANS) + (M_PI * 2 / 6);
	t[2].x = centre_x + .95 * radius_x * cos(tangle2);
	t[2].y = centre_y + .95 * radius_y * sin(tangle2);
#if 0
	printf("texcoords:[%f,%f]->[%f,%f](%f,%f)\n", t[0].x, t[0].y, t[1].x, t[1].y, texture->max_tx, texture->max_ty);
#endif
}
#endif

#if 1
/* new lissajous movement pattern */
static void
calculate_texture_coords(ModeInfo *mi, texture *texture, vector2f t[3]) {

	/* equilateral triangle rotating around centre */
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];
	GLfloat width = texture->max_tx - texture->min_tx;
	GLfloat height = texture->max_ty - texture->min_ty;
	/* centre */
	GLfloat centre_x = texture->min_tx + (width * .5);
	GLfloat centre_y = texture->min_ty + (height * .5);
	/* m radius and t radius should be = .5 */
	/* triangle radius is 30% available space */
	GLfloat t_radius_x = width * .3;
	GLfloat t_radius_y = height * .3;
	/* movement radius is 30% available space */
	GLfloat m_radius_x = width * .2;
	GLfloat m_radius_y = height * .2;
	GLfloat angle2;

	/* centre of triangle */
	GLfloat angle = (gp->ymouse * 2 * M_PI) + (gp->tangle * RADIANS);	/* to radians */
	GLfloat t_centre_x = centre_x + m_radius_x * cos(texture->x_period * angle + texture->x_phase);
	GLfloat t_centre_y = centre_y + m_radius_y * sin(texture->y_period * angle + texture->y_phase);

#if 0
	printf("WH: %f, %f - tWH: %f, %f\n", width, height, texture->width, texture->height);
	printf("size: (%f, %f)\n", width, height);
	printf("centre: (%f, %f)\n", centre_x, centre_y);
#endif

	angle2 = texture->r_period * angle + texture->r_phase;
	t[0].x = t_centre_x + t_radius_x * cos(angle2);
	t[0].y = t_centre_y + t_radius_y * sin(angle2);
	t[1].x = t_centre_x + t_radius_x * cos(angle2 + ANGLE_120);
	t[1].y = t_centre_y + t_radius_y * sin(angle2 + ANGLE_120);
	t[2].x = t_centre_x + t_radius_x * cos(angle2 + ANGLE_240);
	t[2].y = t_centre_y + t_radius_y * sin(angle2 + ANGLE_240);

#if 0
	printf("texcoords:[%f,%f]->[%f,%f](%f,%f)\n", t[0].x, t[0].y, t[1].x, t[1].y, texture->max_tx, texture->max_ty);
#endif
}
#endif
 
#if 0
/* corners into corners - meant to maximise coverage */
static void
calculate_texture_coords(ModeInfo *mi, texture *texture, vector2f t[3]) {

	/* equilateral triangle rotating around centre */
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];
	GLfloat width = texture->max_tx - texture->min_tx;
	GLfloat height = texture->max_ty - texture->min_ty;
	/* centre */
	GLfloat centre_x = texture->min_tx + (width * .5);
	GLfloat centre_y = texture->min_ty + (height * .5);
	/* m radius and t radius should be = .5 */
	/* triangle radius calculated using maths 8) */
#define TRADIUS (M_SQRT2 - 1.0)
#define MRADIUS (1.0 - (M_SQRT2 / 2.0))
	GLfloat t_radius_x = width * TRADIUS * .95;
	GLfloat t_radius_y = height * TRADIUS * .95;
	/* movement radius also calculated using maths */
	GLfloat m_radius_x = width * MRADIUS * .95;
	GLfloat m_radius_y = height * MRADIUS * .95;
	GLfloat angle, angle2;
	GLfloat t_centre_x, t_centre_y;

	/* centre of triangle */
	angle = gp->tangle * RADIANS;	/* to radians */
	t_centre_x = centre_x + m_radius_x * cos(angle);
	t_centre_y = centre_y + m_radius_y * sin(angle);
#if 0
	printf("angle: %f, %f\n", angle, gp->tangle);
	printf("tcentre: %f,%f\n", t_centre_x, t_centre_y);
	printf("tradius: %f,%f\n", t_radius_x, t_radius_y);

	printf("size: (%f, %f)\n", width, height);
	printf("centre: (%f, %f)\n", centre_x, centre_y);
	printf("centre: (%f, %f)\n", centre_x, centre_y);
	printf("TRADIUS: %f\n", TRADIUS);
	printf("MRADIUS: %f\n", MRADIUS);
#endif

	/* angle2 is tied to tangle */
	angle2 = (180.0 - ((30.0 / 90.0) * gp->tangle)) * RADIANS; 
#if 0
	printf("Angle1: %f\tAngle2: %f\n", angle / RADIANS, angle2 / RADIANS);
#endif
	t[0].x = t_centre_x + t_radius_x * cos(angle2);
	t[0].y = t_centre_y + t_radius_y * sin(angle2);
	t[1].x = t_centre_x + t_radius_x * cos(angle2 + ANGLE_120);
	t[1].y = t_centre_y + t_radius_y * sin(angle2 + ANGLE_120);
	t[2].x = t_centre_x + t_radius_x * cos(angle2 + ANGLE_240);
	t[2].y = t_centre_y + t_radius_y * sin(angle2 + ANGLE_240);

#if 0
	printf("texcoords:[%f,%f][%f,%f][%f,%f]\n", t[0].x, t[0].y, t[1].x, t[1].y, t[2].x, t[2].y);
#endif
}
#endif

static int
draw_hexagons(ModeInfo *mi, int translucency, texture *texture)
{
    int polys = 0;
	int		i;
	vector2f t[3];
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];

	calculate_texture_coords(mi, texture, t);

	glColor4f(1.0, 1.0, 1.0, (float)translucency / MAX_FADE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glBindTexture(GL_TEXTURE_2D, texture->id);

	if (gp->list == -1) {
		gp->list = glGenLists(1);
	}

	/* compile new list */
	glNewList(gp->list, GL_COMPILE);
	glBegin(GL_TRIANGLES);

	/*
	** six triangles to each hexagon
	*/

	glTexCoord2f(t[0].x, t[0].y);
	VERTEX0;
	glTexCoord2f(t[1].x, t[1].y);
	VERTEX1;
	glTexCoord2f(t[2].x, t[2].y);
	VERTEX6;

	glTexCoord2f(t[0].x, t[0].y);
	VERTEX0;
	glTexCoord2f(t[2].x, t[2].y);
	VERTEX6;
	glTexCoord2f(t[1].x, t[1].y);
	VERTEX5;

	glTexCoord2f(t[0].x, t[0].y);
	VERTEX0;
	glTexCoord2f(t[1].x, t[1].y);
	VERTEX5;
	glTexCoord2f(t[2].x, t[2].y);
	VERTEX4;

	glTexCoord2f(t[0].x, t[0].y);
	VERTEX0;
	glTexCoord2f(t[2].x, t[2].y);
	VERTEX4;
	glTexCoord2f(t[1].x, t[1].y);
	VERTEX3;

	glTexCoord2f(t[0].x, t[0].y);
	VERTEX0;
	glTexCoord2f(t[1].x, t[1].y);
	VERTEX3;
	glTexCoord2f(t[2].x, t[2].y);
	VERTEX2;

	glTexCoord2f(t[0].x, t[0].y);
	VERTEX0;
	glTexCoord2f(t[2].x, t[2].y);
	VERTEX2;
	glTexCoord2f(t[1].x, t[1].y);
	VERTEX1;

	glEnd();
	glEndList();

	/* call the list n times */
	for (i = 0 ; i < sizeof(hex) / sizeof(hex[0]) ; i++) {

		glPushMatrix();

		glTranslatef(hex[i].x, hex[i].y, 0.0);
		glCallList(gp->list);
        polys += 6;

		glPopMatrix();
	}
	
#ifdef DISPLAY_TEXTURE
	glPushMatrix();
	/* acd debug - display (bigger, centred) texture */
	glScalef(2.0, 2.0, 2.0);
	glTranslatef(-0.5, -0.5, 0.0);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(0.0, 0.0, -0.1);
	glTexCoord2f(1.0, 0.0);
	glVertex3f(1.0, 0.0, -0.1);
	glTexCoord2f(1.0, 1.0);
	glVertex3f(1.0, 1.0, -0.1);
	glTexCoord2f(0.0, 1.0);
	glVertex3f(0.0, 1.0, -0.1);
    polys++;
	glEnd();
	/* acd debug - display texture triangle */
	glColor4f(1.0, 0.5, 1.0, 1.0);
	glBegin(GL_LINE_LOOP);
	glVertex3f(t[0].x, t[0].y, -0.11);
	glVertex3f(t[1].x, t[1].y, -0.11);
	glVertex3f(t[2].x, t[2].y, -0.11);
    polys++;
	glEnd();
	glPopMatrix();
#endif

	glDisable(GL_TEXTURE_2D);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
    return polys;
}

/*
 * main rendering loop
 */
static void
draw(ModeInfo * mi)
{
	GLfloat	x_angle, y_angle, z_angle;
	gleidestruct *gp = &gleidescope[MI_SCREEN(mi)];
	vectorf	v1;

    mi->polygon_count = 0;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	gp->tic += 0.005f;

	x_angle = gp->cam_x_phase + gp->tic * gp->cam_x_speed;
	y_angle = gp->cam_y_phase + gp->tic * gp->cam_y_speed;
	z_angle = gp->cam_z_phase + gp->tic * gp->cam_z_speed;

	if (move) {
		v1.x = 1 * sin(x_angle);
		v1.y = 1 * sin(y_angle);
	} else {
		v1.x = 0;
		v1.y = 0;
	}

	/* size is changed in pinit() to be distance from plane */
	gp->size = MI_SIZE(mi);
	if (gp->size > 10) {
		gp->size = 10;
	}
	if (gp->size <= 0) {
		gp->size = 0;
	}
	if (gp->size > 0) {
		/* user defined size */
		v1.z = gp->size;
	} else if (zoom) {
		/* max distance given by adding the constant and the multiplier */
		v1.z = 5.0 + 3.0 * sin(z_angle);
	} else {
		/* default */
		v1.z = 7.0;
	}

	/* update rotation angle (but not if mouse button down) */
	if (rotate && !gp->button_down_p)
	{
		float	new_rangle_vel = 0.0;

		/* update camera rotation angle and velocity */
		gp->rangle += gp->rangle_vel;
		new_rangle_vel = gp->rangle_vel + gp->rangle_acc;
		if (new_rangle_vel > -MAX_ANGLE_VEL && new_rangle_vel < MAX_ANGLE_VEL)
		{
			/* new velocity is within limits */
			gp->rangle_vel = new_rangle_vel;
		}

		/* randomly change twisting speed - 3ff = 1024 */
		if ((random() % TWISTING_PROBABILITY) < 1.0) {
			gp->rangle_acc = INITIAL_ANGLE_ACC * frand(1.0);
			if (gp->rangle_vel > 0.0) {
				gp->rangle_acc = -gp->rangle_acc;
			}
		}
	}
#if 0
	printf("Rangle: %f : %f : %f\n", gp->rangle, gp->rangle_vel, gp->rangle_acc);
	printf("Tangle: %f : %f : %f\n", gp->tangle, gp->tangle_vel, gp->tangle_acc);
#endif

#ifdef WOBBLE
	/* this makes the image wobble - requires -move and a larger grid */
	gluLookAt(0, 0, v1.z, v1.x, v1.y, 0.0, 0.0, 1.0, 0.0);
#else
	/* no wobble - camera always perpendicular to grid */

	/* rotating camera rather than entire space - smoother */
	gluLookAt(
			v1.x, v1.y, v1.z,
			v1.x, v1.y, 0.0,
			sin((gp->xmouse * M_PI * 2) + gp->rangle * RADIANS),
			cos((gp->xmouse * M_PI * 2) + gp->rangle * RADIANS),
			0.0);
#endif

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
    {
      GLfloat h = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
      int o = (int) current_device_rotation();
      if (o != 0 && o != 180 && o != -180)
        glScalef (1/h, 1/h, 1/h);
    }
# endif

	if (gp->fade == 0)
	{
		/* not fading */
      mi->polygon_count += 
        draw_hexagons(mi, MAX_FADE, &gp->textures[gp->visible]);
	}
	else
	{
		/* fading - show both textures with alpha
		NB first is always max alpha */
        mi->polygon_count += 
          draw_hexagons(mi, MAX_FADE, &gp->textures[1 - gp->visible]);
        mi->polygon_count += 
		  draw_hexagons(mi, MAX_FADE - gp->fade, &gp->textures[gp->visible]);

		/* fade some more */
		gp->fade++;

		/* have we faded enough? */
		if (gp->fade > MAX_FADE)
		{
			/* stop fading */
			gp->fade = 0;
			gp->visible = 1 - gp->visible;
		}
	}

	/* increment texture angle based on time, velocity etc */
	/* but only if button is not down */
	if (!gp->button_down_p)
	{
		float		new_tangle_vel = 0.0;

		gp->tangle += gp->tangle_vel;

		/* work out new texture angle velocity */
		new_tangle_vel = gp->tangle_vel + gp->tangle_acc;
		if (new_tangle_vel > -MAX_ANGLE_VEL && new_tangle_vel < MAX_ANGLE_VEL)
		{
			/* new velocity is inside limits */
			gp->tangle_vel = new_tangle_vel;
		}

		/* randomly change twisting speed - 3ff = 1024 */
		if ((random() % TWISTING_PROBABILITY) < 1.0) {
			gp->tangle_acc = INITIAL_ANGLE_ACC * frand(1.0);
			if (gp->tangle_vel > 0.0) {
				gp->tangle_acc = -gp->tangle_acc;
			}
		}
	}

	glFlush();
}

/* 
 * new window size or exposure 
 */
ENTRYPOINT void reshape_gleidescope(ModeInfo *mi, int width, int height)
{
	GLfloat		h = (GLfloat) height / (GLfloat) width;

	glViewport(0, 0, (GLint) width, (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(50.0, 1/h, 0.1, 2000.0);
	glMatrixMode (GL_MODELVIEW);
	glLineWidth(1);
	glPointSize(1);   
}

static void
pinit(ModeInfo * mi)
{
	gleidestruct	*gp = &gleidescope[MI_SCREEN(mi)];

	/* set start time - star_time = 0 implies non-dynamic texture */
	gp->start_time = (time_t)0;

	/* set the texture size to default */
	/*
	gp->max_tx = 1.0;
	gp->max_ty = 1.0;
	*/

	/* no fading */
	gp->fade = 0;

	glShadeModel(GL_SMOOTH);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);

	/* space for textures */
	glGenTextures(1, &gp->textures[0].id);
	glGenTextures(1, &gp->textures[1].id);
	gp->visible = 0;

	setup_texture(mi, &gp->textures[gp->visible]);

	/*
	**	want to choose a value for arg randomly if neither -arg nor -no-arg
	**	is specified. xscreensaver libraries don't seem to let you do this -
	**	if something isn't true then it is false (pesky two-state boolean values).
	**	so, i've defined both -arg and -no-arg to arguments and added the 
	**	following logic.
	**	(btw if both -arg and -no-arg are defined then arg is set to False)
	*/
	if (zoom == False && nozoom == False)
	{
		/* no zoom preference - randomise */
		zoom = (((random() & 0x1) == 0x1) ? True : False);
	}
	else if (nozoom == True)
	{
		/* definately no zoom */
		zoom = False;
	}

	if (move == False && nomove == False)
	{
		/* no move preference - randomise */
		move = (((random() & 0x1) == 0x1) ? True : False);
	}
	else if (nomove == True)
	{
		/* definately no move */
		move = False;
	}

	if (rotate == False && norotate == False)
	{
		/* no rotate preference - randomise */
		rotate = (((random() & 0x1) == 0x1) ? True : False);
	}
	else if (norotate == True)
	{
		/* definately no rotate */
		rotate = False;
	}

	/* define cam variables */
	gp->cam_x_speed = MAX_CAM_SPEED * frandrange(-.5, 0.5);
	gp->cam_x_phase = random() % 360;
	gp->cam_y_speed = MAX_CAM_SPEED * frandrange(-.5, 0.5);
	gp->cam_y_phase = random() % 360;
	gp->cam_z_speed = MAX_CAM_SPEED * frandrange(-.5, 0.5);
	gp->cam_z_phase = random() % 360;

	/* initial angular speeds */
	gp->rangle_vel = INITIAL_ANGLE_VEL * frandrange(-.5, 0.5);
	gp->tangle_vel = INITIAL_ANGLE_VEL * frandrange(-.5, 0.5);
	gp->rangle_acc = INITIAL_ANGLE_ACC * frandrange(-.5, 0.5);
	gp->tangle_acc = INITIAL_ANGLE_ACC * frandrange(-.5, 0.5);

    /* jwz */
#if 0
    {
      GLfloat speed = 15;
      gp->rangle_vel *= speed;
      gp->tangle_vel *= speed;
      gp->rangle_acc *= speed;
      gp->tangle_acc *= speed;
    }
#endif

	/* distance is 11 - size */
	if (gp->size != -1) {
		if (zoom) {
			fprintf(stderr, "-size given. ignoring -zoom.\n");
			zoom = False;
		}
		if (gp->size < 1) {
			gp->size = 1;
		} else if (gp->size >= 10) {
			gp->size = 10;
		}
		gp->size = 11 - gp->size;
	}

#ifdef DEBUG
printf("phases [%d, %d, %d]\n", gp->cam_x_phase, gp->cam_y_phase, gp->cam_z_phase);
#endif
}

ENTRYPOINT void
init_gleidescope(ModeInfo * mi)
{
	gleidestruct *gp;
	int screen = MI_SCREEN(mi);

	if (gleidescope == NULL) {
		gleidescope = (gleidestruct *) calloc(MI_NUM_SCREENS(mi), sizeof (gleidestruct));
		if (gleidescope == NULL) {
			return;
		}
	}
	gp = &gleidescope[screen];
	gp->window = MI_WINDOW(mi);
    gp->size = -1;
    gp->list = -1;

	if ((gp->glx_context = init_GL(mi)) != NULL) {

		reshape_gleidescope(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
        clear_gl_error(); /* WTF? sometimes "invalid op" from glViewport! */

		glDrawBuffer(GL_BACK);

		/* do initialisation */
		pinit(mi);

	} else {
		MI_CLEARWINDOW(mi);
	}
}

ENTRYPOINT void
draw_gleidescope(ModeInfo * mi)
{
	gleidestruct	*gp = &gleidescope[MI_SCREEN(mi)];
	Display		*display = MI_DISPLAY(mi);
	Window		window = MI_WINDOW(mi);


	if (!gp->glx_context)
		return;

    /* Just keep running before the texture has come in. */
    /* if (gp->waiting_for_image_p) return; */

	glDrawBuffer(GL_BACK);

	glXMakeCurrent(display, window, *(gp->glx_context));
	draw(mi);

	if (mi->fps_p) {
		do_fps (mi);
	}

	glFinish();
	glXSwapBuffers(display, window);

#ifdef GRAB
	if (grab) {
		grab_frame(display, window);
	}
#endif

	/* need to change texture? */
	if ((gp->start_time != 0) && (duration != -1) && gp->fade == 0) {
		if (gp->start_time + duration <= time((time_t *)0)) {
#ifdef DEBUG
			printf("Start Time: %lu - Current Time: %lu\n", (unsigned long)gp->start_time, (unsigned long)time((time_t *)0));
			printf("Changing Texture\n");
#endif
			/* get new snapshot (into back buffer) and start fade count */
			setup_texture(mi, &gp->textures[1 - gp->visible]);
			/* restart fading */
			gp->fade = 1;
		}
	}
}

ENTRYPOINT void
release_gleidescope(ModeInfo * mi)
{
	if (gleidescope != NULL) {
		int screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			gleidestruct *gp = &gleidescope[screen];

			/* acd -  is this needed? */
			if (gp->glx_context) {
				/* Display lists MUST be freed while their glXContext is current. */
				glXMakeCurrent(MI_DISPLAY(mi), gp->window, *(gp->glx_context));

				/* acd - was code here for freeing things that are no longer in struct */
			}
		}
		(void) free((void *) gleidescope);
		gleidescope = NULL;
	}
	
	FreeAllGL(mi);
}

XSCREENSAVER_MODULE ("Gleidescope", gleidescope)

#endif
