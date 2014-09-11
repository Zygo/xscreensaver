/* timetunnel. Based on dangerball.c, hack by Sean Brennan <zettix@yahoo.com>*/
/* dangerball, Copyright (c) 2001-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define GL_GLEXT_PROTOTYPES 1

#include <math.h> /* for log2 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        30          \n" \
			"*showFPS:      False       \n" \
			"*timeStart:     0.0       \n" \
			"*timeEnd:       27.79       \n" \
			"*wireframe:    False       \n" \



# define refresh_tunnel 0
# define release_tunnel 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"

#define DEF_START	"0.00"
#define DEF_DILATE	"1.00"
#define DEF_END		"27.79"
#define DEF_LOCKLOGO	"False"
#define DEF_DRAWLOGO	"True"
#define DEF_REVERSE	"False"
#define DEF_FOG		"True"
#define DEF_TEXTURE     "True"
#define MAX_TEXTURE 10
#define CYL_LEN	    	14.0
#define DIAMOND_LEN	10.0

static float start, end, dilate;
static Bool do_texture, drawlogo, wire, reverse, do_fog;
static const char *do_tx1, *do_tx2, *do_tx3, *do_tun1, *do_tun2, *do_tun3;

static XrmOptionDescRec opts[] = {
  {"-texture"	, ".texture",   XrmoptionNoArg, "true" },
  {"+texture"	, ".texture",   XrmoptionNoArg, "false" },
  {"-start"	, ".start",    	XrmoptionSepArg, 0 },
  {"-end"	, ".end",    	XrmoptionSepArg, 0 },
  {"-dilate"	, ".dilate",   XrmoptionSepArg, 0 },
  {"+logo"	, ".drawlogo",   XrmoptionNoArg, "false" },
  {"-reverse"	, ".reverse",   XrmoptionNoArg, "true" },
  {"+fog"	, ".fog",  	XrmoptionNoArg, "false" },
  {"-marquee"   , ".marquee", XrmoptionSepArg, 0},
  /* {"+marquee"   , ".marquee", XrmoptionNoArg, "(none)"}, */
  {"-tardis"   , ".tardis", XrmoptionSepArg, 0},
  /* {"+tardis"   , ".tardis", XrmoptionNoArg, "(none)"}, */
  {"-head"   , ".head", XrmoptionSepArg, 0},
  /* {"+head"   , ".head", XrmoptionNoArg, "(none)"}, */
  {"-tun1"   , ".tun1", XrmoptionSepArg, 0},
  /* {"+tun1"   , ".tun1", XrmoptionNoArg, "(none)"}, */
  {"-tun2"   , ".tun2", XrmoptionSepArg, 0},
  /* {"+tun2"   , ".tun2", XrmoptionNoArg, "(none)"}, */
  {"-tun3"   , ".tun3", XrmoptionSepArg, 0},
  /* {"+tun3"   , ".tun3", XrmoptionNoArg, "(none)"}, */
};

static argtype vars[] = {
  {&do_texture, "texture", "Texture", DEF_TEXTURE, t_Bool},
  {&start, "start", "Start", DEF_START, t_Float},
  {&end,     "end",   "End", DEF_END  , t_Float},
  {&dilate,     "dilate",   "Dilate", DEF_DILATE  , t_Float},
  {&drawlogo,     "drawlogo",   "DrawLogo", DEF_DRAWLOGO  , t_Bool},
  {&reverse,     "reverse",   "Reverse", DEF_REVERSE  , t_Bool},
  {&do_fog,     "fog",  "Fog", DEF_FOG  , t_Bool},
  {&do_tx1,	"marquee", "Marquee", "(none)", t_String},
  {&do_tx2,	"tardis", "Tardis", "(none)", t_String},
  {&do_tx3,	"head",	"Head", "(none)", t_String},
  {&do_tun1,	"tun1",	"Tunnel 1", "(none)", t_String},
  {&do_tun2,	"tun2",	"Tunnel 2", "(none)", t_String},
  {&do_tun3,	"tun3",	"Tunnel 3", "(none)", t_String},
};

ENTRYPOINT ModeSpecOpt tunnel_opts = {countof(opts), opts, countof(vars), vars, NULL};
#include "xpm-ximage.h"
#include "images/logo-180.xpm"
#include "images/tunnelstar.xpm"
#include "images/timetunnel0.xpm"
#include "images/timetunnel1.xpm"
#include "images/timetunnel2.xpm"


#ifdef USE_GL /* whole file */

/* ANIMATION CONTROLS */
/* an effect is a collection of floating point variables that vary with time.
A knot is a timestamp with an array of floats.  State is the current values of the floats.
State is set by linearly interpolating between knots */
typedef struct {
	float *knots, *state;
	int numknots, knotwidth;
	float direction;
} effect_t;

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  int time_oldusec, time_oldsec;

  int num_texshifts; /* animates tunnels. Not an effect. */
  GLfloat pos, *texshift;

  GLuint texture_binds[MAX_TEXTURE], cyllist, diamondlist;

  float effect_time, effect_maxsecs; /* global time controls */
  float start_time, end_time;

  int num_effects;
  effect_t *effects; /* array of all effects */

} tunnel_configuration;

static tunnel_configuration *tconf = NULL;

/* allocate memory and populate effect with knot data */
static void init_effect(effect_t *e, int numk, int kwidth, 
	float dir, float *data ) 
{
	int i, j;

	e->numknots = numk;	
	e->knotwidth = kwidth;	
	e->direction = dir;
	e->knots = calloc(numk * kwidth, sizeof(float));
	e->state = calloc(numk, sizeof(float));
	for ( i = 0 ; i < e->numknots ; i++)
		for ( j = 0 ; j < e->knotwidth; j++)
			e->knots[i * kwidth + j] = data[i * kwidth + j];
}

/* static knot data. each effect is listed and knot data is hard coded. 
   Knots are linerally interpolated to yield float values, depending on
   knot width.  knot format is [time, data, data, data...].
   Data can be alpha, zvalue, etc. */
static void init_effects(effect_t *e, int effectnum)
{
	/* effect 1: wall tunnel. percent closed */
	float e1d[6][2] = 
		{{0.0, 0.055}, 
		 {2.77, 0.055}, 
		 {3.07,1.0}, 
		 {8.08, 1.0},
		 {8.08, 0.0}, 
		 {10.0, 0.0}};
	/* effect 2: tardis. distance and alpha */
	float e2d[8][3] = 
	{	{0.0, 0.0 , 0.0}, 
		{3.44, 0.0 , 0.0},
		{3.36, 5.4 , 0.0},
		{4.24, 3.66, 1.0},
		{6.51, 2.4,  0.94},
		{8.08, 0.75 , 0.0}, 
		{8.08, 0.0 , 0.0},
		{10.0, 0.0, 0.0}};
	/* effect 3: cylinder. alpha  */
	float e3d[5][2] = 
		{{0.0, 0.0}, 
		 {6.41, 0.00},
		 {8.08, 1.0}, 
		 {14.81, 1.0},
		 {15.65, 0.0}};

	/* effect 4: fog. color, density,  start, end  */
	float e4d[9][5] = 
		{{0.0 , 1.0, 0.45, 3.0, 15.0},
		 {6.40, 1.0, 0.45, 3.0, 14.0},
		 {8.08, 1.0, 0.95, 1.0, 14.0},
		 {15.17, 1.0, 0.95, 1.0, 6.0},
		 {15.51, 1.0, 0.95, 3.0, 8.0},
		 {23.35, 1.0, 0.95, 3.0, 8.0},
		 {24.02, 0.0, 0.95, 2.3, 5.0},
		 {26.02, 0.0, 0.95, 2.3, 5.0},
		 {27.72, 0.0, 1.00, 0.3, 0.9}
		 };

	/* effect 5: logo. dist, alpha  */
	float e5d[7][3] = 
		{{0.0, 0.0, 0.0}, 
		{16.52, 0.00, 0.0}, 
		{16.52, 0.80, 0.01}, 
		{17.18, 1.15, 1.0}, 
		{22.36, 5.3, 1.0}, 
		{22.69, 5.7, 0.0},
		{22.69, 0.0, 0.0}
		};
	/* effect 6: diamond tunnel. alpha */
	float e6d[3][2] = 
		{{0.0, 0.00}, 
		{15.17, 0.00},
		{15.51,1.0}};

	/* effect 7: tardis cap draw . positive draws cap*/
	float e7d[3][2] = 
		{{0.0, -1.00}, 
		{4.24, -1.00},
		{4.24, 1.00}};

	/* effect 8: star/asterisk: alpha */
	float e8d[5][2] = 
		{{0.0,    .00}, 
		{10.77,   .00},
		{11.48,  1.00},
		{15.35,  1.00},
		{16.12,  0.00}};

	/* effect 9: whohead 1  alpha */
	float e9d[5][2] = 
		{{0.0,    .00}, 
		{13.35,   .00},
		{14.48,  1.00},
		{15.17,  1.00},
		{15.97,  0.00}};
		/* {14.87,  1.00},
		{15.17,  0.00}}; */

	/* effect 10: whohead-brite  alpha */
	float e10d[5][2] = 
		{{0.0,    .00}, 
		{11.34,   .00},
		{12.34,   .20},
		{13.35,  0.60},
		{14.48,  0.00}}; 
		/* {13.95,  0.00}}; */

	/* effect 11: whohead-psy  alpha */
	float e11d[5][2] = 
		{{0.0,    .00}, 
		{14.87,   .00},
		{15.17,  1.00},
		{15.91,  0.00},
		{16.12,  0.00}};

	/* effect 12: whohead-silhouette pos-z,  alpha */
	float e12d[6][3] = 
		{{0.0,   1.0,  .00}, 
		{15.07,  1.0, 0.00},
		{15.07,  1.0, 1.00},
		{16.01,  1.0, 1.00},
		{16.78,  0.5, 1.00},
		{16.78,  0.1, 0.00} };

	/* effect 1: wall tunnel */
	if (effectnum == 1)
		init_effect(e, 6, 2,  -0.2, (float *) e1d);

	/* effect 2: tardisl */
	if (effectnum == 2)
		init_effect(e, 8, 3, 1.0,  (float *) e2d);

	/* effect 3: cylinder tunnel  */
	if (effectnum == 3)
		init_effect(e, 5, 2, 0.889  ,  (float *) e3d);

	/* effect 4: fog color */
	if (effectnum == 4)
		init_effect(e, 9, 5, 1.0,  (float *) e4d);
	/* effect 5: logo distance, alpha*/
	if (effectnum == 5)
		init_effect(e, 7, 3, 1.0,  (float *) e5d);
	/* effect 6: diamond tunnel, alpha*/
	if (effectnum == 6)
		init_effect(e, 3, 2, 0.24 ,  (float *) e6d);

	/* effect 7: cap wall tunnel*/
	if (effectnum == 7)
		init_effect(e, 3, 2, 1.0,  (float *) e7d);

	/* effect 8: asterisk */
	if (effectnum == 8)
		init_effect(e, 5, 2, 1.0,  (float *) e8d);

	/* effect 9, 10, 11, 12: whoheads */
	if (effectnum == 9 )
		init_effect(e, 5, 2, 1.0,  (float *) e9d);
	if (effectnum == 10 )
		init_effect(e, 5, 2, 1.0,  (float *) e10d);
	if (effectnum == 11 )
		init_effect(e, 5, 2, 1.0,  (float *) e11d);
	if (effectnum == 12 )
		init_effect(e, 6, 3, 1.0,  (float *) e12d);
}


/* set fog parameters, controlled by effect */
static void update_fog(float color, float density, float start, float end) 
{
		GLfloat col[4];
	
		col[0] = col[1] = col[2] = color;
		col[3] = 1.0;

      		glFogi(GL_FOG_MODE, GL_LINEAR);
      		glFogfv(GL_FOG_COLOR, col);
      		glFogf(GL_FOG_DENSITY, density);
      		glFogf(GL_FOG_START, start);
      		glFogf(GL_FOG_END, end);
}

/* set effect's floating point data values by linearally interpolating
between two knots whose times bound the current time: eff_time */

static void update_knots(effect_t *e, float eff_time) 
{
	int i, j;
	float timedelta, lowknot, highknot, *curknot, *nextknot;

	for ( i = 0 ; i < e->numknots ; i++)
		if (e->knots[i * e->knotwidth] <= eff_time) {
			if ( i < e->numknots - 1) 
				nextknot = e->knots + (i + 1) * e->knotwidth;
			else
				/*repeat last knot to carry knot data forward*/
				nextknot = e->knots + (i) * e->knotwidth;
			curknot = e->knots + i * e->knotwidth;
			if (*nextknot - *curknot <= 0.0) timedelta = 1.0;
			else
				timedelta = (eff_time-*curknot)/(*nextknot-*curknot);
			if (timedelta > 1.0) timedelta = 1.0;
			for (j = 1 ; j < e->knotwidth ; j++) {
				highknot = (float) *(nextknot + j);
				lowknot  = (float) *(curknot  + j);
				e->state[j - 1 ] = lowknot+(highknot-lowknot)*timedelta;
			}
		}
	
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_tunnel (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (90.0, 1/h, 0.2, 50.0); 

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 0.3,
             0.0, 0.0, 1.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
}




ENTRYPOINT Bool
tunnel_handle_event (ModeInfo *mi, XEvent *event)
{
  tunnel_configuration *tc = &tconf[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, tc->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &tc->button_down_p))
    return True;

  return False;
}

static void setTexParams(void)
{
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

static void update_animation(tunnel_configuration *tc) {

	/* time based, of course*/
	/* shift texture based on elapsed time since previous call*/
	struct timeval tv;
	struct timezone tz;
	int elapsed_usecs, elapsed_secs, i;
	float computed_timeshift;

	/* get new animation time */
	gettimeofday(&tv, &tz);
	elapsed_secs = tv.tv_sec - tc->time_oldsec;
	elapsed_usecs = tv.tv_usec - tc->time_oldusec;
	/* store current time */
 	tc->time_oldsec = tv.tv_sec ;
 	tc->time_oldusec = tv.tv_usec;
	/* elaped time. computed timeshift is tenths of a second */
	computed_timeshift = (float) (elapsed_secs * 1000000. + elapsed_usecs)/ 
						      100000.0;

	/* calibrate effect time to lie between start and end times */
	/* loop if time exceeds end time */
	if (reverse)
		tc->effect_time -= computed_timeshift / 10.0 * dilate;
	else
		tc->effect_time += computed_timeshift / 10.0 * dilate;
	if ( tc->effect_time >= tc->end_time)
		tc->effect_time = tc->start_time;
	if ( tc->effect_time < tc->start_time)
		tc->effect_time = tc->end_time;;

	/* move texture shifters in effect's direction, e.g. tardis
	   tunnel moves backward, effect 1's direction */
	 if (reverse) { 
		tc->texshift[0] -= tc->effects[1].direction * computed_timeshift/ 10.0; 
		tc->texshift[1] -= tc->effects[3].direction * computed_timeshift/ 10.0; 
		tc->texshift[2] -= tc->effects[6].direction * computed_timeshift/ 10.0; 

	} else {
		tc->texshift[0] += tc->effects[1].direction * computed_timeshift/ 10.0; 
		tc->texshift[1] += tc->effects[3].direction * computed_timeshift/ 10.0; 
		tc->texshift[2] += tc->effects[6].direction * computed_timeshift/ 10.0; 
	}

	/* loop texture shifters if necessary */
  	for ( i = 0 ; i < tc->num_texshifts; i++) {
		if (tc->texshift[i] > 1.0)
			tc->texshift[i] -= (int) tc->texshift[i];
		if (tc->texshift[i]< -1.0)
			tc->texshift[i] -= (int) tc->texshift[i];
	}

	/* update effect data with current time. Uses linear interpolation */	
	for ( i = 1 ; i <= tc->num_effects ; i++)
		update_knots(&tc->effects[i], tc->effect_time);

} /*update_animation*/

/* draw a textured(tex) quad at a certain depth (z), and certain alpha (alpha), 
with aspect ratio (aspect), and blending mode (blend_mode) of either adding
or subtracting.  if alpha is zero or less, nothing happens */
static void draw_sign(ModeInfo *mi, tunnel_configuration *tc, float z,  float alpha, float aspect,
		GLuint tex, int blend_mode)
{

#ifndef HAVE_JWZGLES
	if (alpha > 0.0) {
  		mi->polygon_count ++;
		/* glEnable(GL_BLEND); */
		glBlendColor(0.0, 0.0, 0.0, alpha);
		/*glBlendColor(0.0, 0.0, 0.0, 0.0); */
		if (blend_mode == 1) {
			glBlendFunc(GL_CONSTANT_ALPHA,
				    GL_ONE);
			glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
		} else if (blend_mode == 2) {
			glBlendFunc(GL_CONSTANT_ALPHA,
				    GL_ONE);
			glBlendEquation(GL_FUNC_ADD);
		} else {
			glBlendFunc(GL_CONSTANT_ALPHA,
				    GL_ONE_MINUS_CONSTANT_ALPHA);
			glBlendEquation(GL_FUNC_ADD);
		} /* blend mode switch */

#ifdef HAVE_GLBINDTEXTURE
		if (do_texture)
			glBindTexture(GL_TEXTURE_2D, tc->texture_binds[tex]);
#endif
		glBegin(GL_QUADS);
		glTexCoord2f(1.0, 0.0);
		glVertex3f(-1.0 , -1.0 * aspect , z);
		glTexCoord2f(1.0, 1.0);
		glVertex3f(-1.0 , 1.0 * aspect , z);
		glTexCoord2f(0.0, 1.0);
		glVertex3f(1.0 , 1.0 * aspect , z);
		glTexCoord2f(0.0, 0.0);
		glVertex3f(1.0 , -1.0 * aspect , z); 
		glEnd();
		if (blend_mode != 0) {
			glBlendFunc(GL_CONSTANT_ALPHA,
				    GL_ONE_MINUS_CONSTANT_ALPHA);
			glBlendEquation(GL_FUNC_ADD);
		}
		/* glDisable(GL_BLEND); */

	}
#endif /* !HAVE_JWZGLES */
} /* draw sign */


/* draw a time tunnel.  used for both cylinder and diamond tunnels.
   uses texture shifter (indexed by shiftnum) to simulate motion.
   tunnel does not move, and is acutally a display list.  if alpha = 0, skip */
static void draw_cyl(ModeInfo *mi, tunnel_configuration *tc, float alpha, int texnum, int listnum, int shiftnum)
{
#ifndef HAVE_JWZGLES
	if (alpha > 0.0) {
		if (listnum  ==  tc->diamondlist)
  			mi->polygon_count += 4;
		if (listnum  ==  tc->cyllist)
  			mi->polygon_count += 30;
  		glMatrixMode(GL_TEXTURE);
  		glLoadIdentity();
  		glTranslatef(tc->texshift[shiftnum], 0.0, 0.0);
  		glMatrixMode(GL_MODELVIEW);
		/* glEnable(GL_BLEND); */
		glBlendColor(0.0, 0.0, 0.0, alpha);
		glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
	
#ifdef HAVE_GLBINDTEXTURE
		if (do_texture)
			glBindTexture(GL_TEXTURE_2D, tc->texture_binds[texnum]);
#endif
		glCallList(listnum);

  		glMatrixMode(GL_TEXTURE);
  		glLoadIdentity();
  		glMatrixMode(GL_MODELVIEW); 
		/* glDisable(GL_BLEND); */
	}
#endif /* HAVE_JWZGLES */
}


/* make tardis type tunnel.  Starts as walls and then
grows to outline of tardis.  percent is how complete
tardis outline is.  cap is to draw cap for nice fog effects */

static void make_wall_tunnel(ModeInfo *mi, tunnel_configuration *tc, float percent, float cap)
{
	/* tardis is about 2x1, so wrap tex around, starting at the base*/
	/* tex coords are:

 _tl__tr_
 |      |
l|      |r
 |      |
 -bl__br_  
	that's br=bottom right, etc. ttr is top-top-right */

	float   half_floor= 0.08333333333333333,
		full_wall = 0.33333333333333333;
	float   br1,
		r0 , r1 ,
		tr0, tr1,
		tl0, tl1,
		l0 , l1 ,
		depth=0.3, zdepth=15.0;
	/* zdepth is how far back tunnel goes */
	/* depth is tex coord scale.  low number = fast texture shifting */

	float textop, texbot;	
	float height;

	br1 = half_floor;
	r0 = br1 ;
	r1 = r0 + full_wall;
	tr0 = r1;
	tr1 = r1 + half_floor;
	tl0 = tr1;
	tl1 = tl0 + half_floor;
	l0 = tr1;
	l1 = l0 + full_wall;

  	glMatrixMode(GL_TEXTURE);
  	glLoadIdentity();
	glRotatef(90.0, 0.0, 0.0, 1.0);
  	glTranslatef(tc->texshift[0], 0.0, 0.0);
  	glMatrixMode(GL_MODELVIEW);

#ifdef HAVE_GLBINDTEXTURE
	if (do_texture)
        	glBindTexture(GL_TEXTURE_2D, tc->texture_binds[0]);
#endif
	glColor3f(1.0, 1.0, 0.0);
	if (cap > 0.0 && percent > 0.0 && drawlogo && do_fog) {
  		mi->polygon_count += 6;
		glBegin(GL_TRIANGLE_FAN);
		glVertex3f(0.0, 0.0, zdepth);
		glVertex3f(-1.0, -2.0, zdepth);
		glVertex3f(1.0, -2.0, zdepth);
		glVertex3f(1.0, 2.0, zdepth);
		glVertex3f(0.2, 2.0, zdepth);
		glVertex3f(0.2, 2.2, zdepth);
		glVertex3f(-0.2, 2.2, zdepth);
		glVertex3f(-0.2, 2.0, zdepth);
		glVertex3f(-1.0, 2.0, zdepth);
		glVertex3f(-1.0, -2.0, zdepth);
		glEnd();
	}
	if (percent > ( full_wall * 2.0)) {
		glBegin(GL_QUADS);

		height = (percent  - full_wall * 2.0) /( 1.0 - full_wall * 2.0);
		if (height > 1.0) height = 1.0;


		if ( height > 0.8) {
  			mi->polygon_count += 2;
			if ( height > 0.90) {
  				mi->polygon_count += 2;
				/* TTTR */
				texbot = tr0;
				textop = tr0 + half_floor * height;
				glTexCoord2f(0.0, texbot);
				glVertex3f(0.2, 2.2, 0.0);
		
				glTexCoord2f(0.0, textop);
				glVertex3f(2.0 - height * 2.0, 2.2, 0.0);
		
				glTexCoord2f(depth, textop);
				glVertex3f(2.0 - height * 2.0, 2.2, zdepth);
	
				glTexCoord2f(depth, texbot);
				glVertex3f(0.2, 2.2, zdepth);
	
				/* TTTL */
				texbot = tl1 - half_floor * height;
				textop = tl1;
				glTexCoord2f(0.0, texbot);
				glVertex3f(-2.0 + height * 2.0, 2.2, 0.0);
		
				glTexCoord2f(0.0, textop);
				glVertex3f(-0.2, 2.2, 0.0);
		
				glTexCoord2f(depth, textop);
				glVertex3f(-0.2, 2.2, zdepth);
		
				glTexCoord2f(depth, texbot);
				glVertex3f(-2.0 + height * 2.0, 2.2, zdepth);
			}
			if (height > 0.90) height = 0.90;

			/* TTR */
			texbot = tr0;
			textop = tr0 + half_floor * height;
			glTexCoord2f(0.0, texbot);
			glVertex3f(0.2, 2.0, 0.0);
	
			glTexCoord2f(0.0, textop);
			glVertex3f(0.2, 0.4 + height * 2.0, 0.0);
	
			glTexCoord2f(depth, textop);
			glVertex3f(0.2, 0.4 + height * 2.0, zdepth);
	
			glTexCoord2f(depth, texbot);
			glVertex3f(0.2, 2.0, zdepth);

			/* TTL */
			texbot = tl1 - half_floor * height;
			textop = tl1;
			glTexCoord2f(0.0, texbot);
			/*glVertex3f(-.2, 2.0 + (0.9 - height) * 2.0, 0.0); */
			glVertex3f(-.2,  0.4 + height * 2.0, 0.0);
	
			glTexCoord2f(0.0, textop);
			glVertex3f(-.2, 2.0, 0.0);
	
			glTexCoord2f(depth, textop);
			glVertex3f(-.2, 2.0, zdepth);
	
			glTexCoord2f(depth, texbot);
			glVertex3f(-.2, 0.4 + height * 2.0, zdepth);
		}
	
		height = (percent  - full_wall * 2.0) /( 1.0 - full_wall * 2.0);
		if (height > 0.8) height = 0.8;


  		mi->polygon_count += 2;
		/* TR */
		texbot = tr0;
		textop = tr0 + half_floor * height;
		glTexCoord2f(0.0, texbot);
		glVertex3f(1.0, 2.0, 0.0);

		glTexCoord2f(0.0, textop);
		glVertex3f(1.0 - height, 2.0, 0.0);

		glTexCoord2f(depth, textop);
		glVertex3f(1.0 - height, 2.0, zdepth);

		glTexCoord2f(depth, texbot);
		glVertex3f(1.0, 2.0, zdepth);

		/* TL */
		texbot = tl1 - half_floor * height;
		textop = tl1;
		glTexCoord2f(0.0, texbot);
		glVertex3f(-1.0 + height, 2.0, 0.0);

		glTexCoord2f(0.0, textop);
		glVertex3f(-1.0, 2.0, 0.0);

		glTexCoord2f(depth, textop);
		glVertex3f(-1.0, 2.0, zdepth);

		glTexCoord2f(depth, texbot);
		glVertex3f(-1.0 + height, 2.0, zdepth);

		height = (percent  - full_wall * 2.0) /( 1.0 - full_wall * 2.0);

		if (height > 1.0) height = 1.0;


  		mi->polygon_count += 2;
		/* BR */
		texbot = tr0;
		textop = tr0 + half_floor * height;
		glTexCoord2f(0.0, texbot);
		glVertex3f(1.0, -2.0, 0.0);

		glTexCoord2f(0.0, textop);
		glVertex3f(1.0 - height, -2.0, 0.0);

		glTexCoord2f(depth, textop);
		glVertex3f(1.0 - height, -2.0, zdepth);

		glTexCoord2f(depth, texbot);
		glVertex3f(1.0, -2.0, zdepth);

		/* BL */
		texbot = tl1 - half_floor * height;
		textop = tl1;
		glTexCoord2f(0.0, texbot);
		glVertex3f(-1.0 + height, -2.0, 0.0);

		glTexCoord2f(0.0, textop);
		glVertex3f(-1.0, -2.0, 0.0);

		glTexCoord2f(depth, textop);
		glVertex3f(-1.0, -2.0, zdepth);

		glTexCoord2f(depth, texbot);
		glVertex3f(-1.0 + height, -2.0, zdepth);

		
		glEnd();
	}
	
	if (percent > 0.0) {
  		mi->polygon_count += 2;
		glBegin(GL_QUADS);
		height = percent / ( full_wall * 2.0);
		if (height > 1.0) height = 1.0;
		textop = (l0 + l1) / 2.0 - full_wall * 0.5 * height;
		texbot = (l0 + l1) / 2.0 + full_wall * 0.5 * height;

		glTexCoord2f(0.0, textop);
		glVertex3f(-1.0, height * 2, 0.0);

		glTexCoord2f(0.0, texbot);
		glVertex3f(-1.0, -height * 2, 0.0);

		glTexCoord2f(depth, texbot);
		glVertex3f(-1.0, -height * 2, zdepth);

		glTexCoord2f(depth, textop);
		glVertex3f(-1.0, height * 2, zdepth);

		textop = (r0 + r1) / 2.0 - full_wall * 0.5 * height;
		texbot = (r0 + r1) / 2.0 + full_wall * 0.5 * height;

		glTexCoord2f(0.0, texbot);
		glVertex3f(1.0, height * 2, 0.0);

		glTexCoord2f(0.0, textop);
		glVertex3f(1.0, -height * 2, 0.0);

		glTexCoord2f(depth, textop);
		glVertex3f(1.0, -height * 2, zdepth);

		glTexCoord2f(depth, texbot);
		glVertex3f(1.0, height * 2, zdepth);
		glEnd();
	}


  	glMatrixMode(GL_TEXTURE);
  	glLoadIdentity();
  	glMatrixMode(GL_MODELVIEW);
} /* make_wall_tunnel */

/* wraps an int to between min and max.
   Kind of like the remainder when devided by (max - min).
   Used to create torus mapping on square array */
static int wrapVal(int val, int min, int max)
{
	int ret;

	ret = val;
	if (val >= max)
		ret = min + (val - max ) % (max - min);
	if (val < min)
		ret = max - (min - val) % (max - min);
	return(ret);
}

/*=================== Load Texture =========================================*/
/* ripped from atunnel.c,  Copyright (c) E. Lassauge, 2003-2004. */
/* modified like so by Sean Brennan:
  take texture object for glbind
  removed xlock stuff
  Added filters:
    blur color / alpha channel [3x3 box filter, done [blur] times
    anegative : create b/w image from zero alpha. zero alpha gets bw_color,
		nonzero alpha gets 1.0 - bwcolor, then alpha flipped to 1-alpha.

  Inputs: xpm structure, or filename of xmp image.  if filename == NULL, use structure.
  Outputs: texture bound to texutre Id texbind.

*/

static float mylog2(float x) { return ( log(x) / log(2));}

static void LoadTexture(ModeInfo * mi, char **fn, const char *filename, GLuint texbind, int blur, float bw_color, Bool anegative, Bool onealpha)
{
	/* looping and temporary array index variables */
	int ix, iy, bx, by, indx, indy, boxsize, cchan, tmpidx, dtaidx;

	float boxdiv, tmpfa, blursum ;
	unsigned char *tmpbuf, tmpa;
	Bool rescale;


        XImage *teximage;    /* Texture data */

	rescale = False;

	boxsize = 2;
	boxdiv = 1.0 / ( boxsize * 2.0 + 1.0) / ( boxsize * 2.0 + 1.0);


	if (filename) 
        	teximage = xpm_file_to_ximage(MI_DISPLAY(mi), MI_VISUAL(mi),
                         MI_COLORMAP(mi), filename);
        else 
		teximage = xpm_to_ximage(MI_DISPLAY(mi), MI_VISUAL(mi),
                         MI_COLORMAP(mi), fn);
	if (teximage == NULL) {
            fprintf(stderr, "%s: error reading the texture.\n", progname);
            glDeleteTextures(1, &texbind);
            do_texture = False;
            exit(0);
        }

	/* check if image is 2^kumquat, where kumquat is an integer between 1 and 10. Recale to
	   nearest power of 2. */
	tmpfa = mylog2((float) teximage->width);
	bx = 2 << (int) (tmpfa -1);
	if (bx != teximage->width) {
		rescale = True;
		if ((tmpfa - (int) tmpfa) >  0.5849)
			bx = bx * 2;
	}
	tmpfa = mylog2((float) teximage->height);
	by = 2 << (int) (tmpfa - 1);
	if (by != teximage->height) {
		rescale = True;
		if ((tmpfa - (int) tmpfa) >  0.5849)
			by = by * 2;
	}

#ifndef HAVE_JWZGLES
	if (rescale) {
		tmpbuf = calloc(bx * by * 4, sizeof(unsigned char));
		if (gluScaleImage(GL_RGBA, teximage->width, teximage->height, GL_UNSIGNED_BYTE, teximage->data,
				bx, by, GL_UNSIGNED_BYTE, tmpbuf))
        		check_gl_error("scale image");
		
		free(teximage->data);
		teximage->data = (char *) tmpbuf;
		teximage->width = bx;
		teximage->height= by;
	}
	/* end rescale code */
#endif /* !HAVE_JWZGLES */
		
	if (anegative ) {
		for (ix = 0 ; ix < teximage->height * teximage->width; ix++)
			{
				if (!teximage->data[ ix * 4 + 3]) {
					teximage->data[ ix * 4 + 3]  = (unsigned char) 0xff;
					tmpa = (unsigned char) (bw_color * 0xff);
			 	} else  {
					if (onealpha)
						teximage->data[ ix * 4 + 3]  = (unsigned char) 0xff;
					else
						teximage->data[ ix * 4 + 3]  = (unsigned char)  0xff - 
								teximage->data[ ix * 4 + 3];
					tmpa = (unsigned char) ((1.0 - bw_color) * 0xff);
				}
				/* make texture uniform b/w color */
				teximage->data[ ix * 4 + 0]  =
					(unsigned char) ( tmpa);
				teximage->data[ ix * 4 + 1]  =
 					(unsigned char) ( tmpa);
				teximage->data[ ix * 4 + 2]  =
 					(unsigned char) ( tmpa);
				/* negate alpha */
			}
	}
		
	if (blur > 0) {
		if (! anegative ) /* anegative alread b/w's the whole image */
			for (ix = 0 ; ix < teximage->height * teximage->width; ix++)
				if (!teximage->data[ ix * 4 + 3])
				{
					teximage->data[ ix * 4 + 0]  =
						(unsigned char) ( 255.0 * bw_color);
					teximage->data[ ix * 4 + 1]  =
 						(unsigned char) ( 255.0 * bw_color);
					teximage->data[ ix * 4 + 2]  =
 						(unsigned char) ( 255.0 * bw_color);
				}
		;
		tmpbuf = calloc(teximage->height * teximage->width * 4, sizeof(unsigned char)  )	;
		while (blur--) {
			/* zero out tmp alpha buffer */
			for (iy = 0 ; iy <teximage->height * teximage->width * 4 ; iy++)
			 	tmpbuf[iy] = 0;
			for (cchan = 0; cchan < 4 ; cchan++) {
				for (iy = 0 ; iy < teximage->height ; iy++) {
					for (ix = 0 ; ix < teximage->width ; ix++) {
						dtaidx = (teximage->width * iy + ix) * 4;
						tmpa =  teximage->data[dtaidx + cchan];
						tmpfa = (float) tmpa * boxdiv;
						/* box filter */
						for (by = -boxsize ; by <= boxsize; by++) {
							for (bx = -boxsize ; bx <= boxsize; bx++) {
								indx = wrapVal(ix + bx, 0, teximage->width);
								indy = wrapVal(iy + by, 0, teximage->height);
								tmpidx = (teximage->width * indy + indx) * 4;
								blursum = tmpfa;
								tmpbuf[tmpidx + cchan] += (unsigned char) blursum;
							} /* for bx */
						} /* for by  */
					} /* for ix  */
				} /* for iy */
			} /* for cchan */
			/* copy back buffer */
			for (ix = 0 ; ix < teximage->height * teximage->width * 4; ix++)
				teximage->data[ix] = tmpbuf[ix];
		} /*while blur */
		free(tmpbuf); /*tidy*/
	} /* if blur */

			
	

        clear_gl_error();
#ifdef HAVE_GLBINDTEXTURE
        glBindTexture(GL_TEXTURE_2D, texbind);
        clear_gl_error(); /* WTF? sometimes "invalid op" from glBindTexture! */
#endif
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, teximage->width, teximage->height,
                        0, GL_RGBA, GL_UNSIGNED_BYTE, teximage->data);
        check_gl_error("texture");
	setTexParams(); 
        XDestroyImage(teximage);
}

/* creates cylinder for time tunnel. sides, zmin, zmax, rad(ius) obvious.
   stretch scales texture coords; makes tunnel go slower the larger it is.
   not drawn, but put into display list. */
static void makecyl(int sides, float zmin, float zmax, float rad, float stretch) 
{
	int i;
	float theta;

	/* cap */
	if (do_fog) {
		glBegin(GL_TRIANGLE_FAN);
		glTexCoord2f(1.0, 0.0);
		glVertex3f(0.0 , 0.0 , zmax); 
		for (i = 0 ; i <= sides; i++) {  
			theta = 2.0 * M_PI * ((float) i / (float) sides);
			glVertex3f(cos(theta) * rad, sin(theta) * rad, zmax);
		}
		glVertex3f(cos(0.0) * rad, sin(0.0) * rad, zmax);
		glEnd(); 
	}
	
	glBegin(GL_QUAD_STRIP);
	for (i = 0 ; i <= sides; i++)
	{
		if ( i != sides) {
			theta = 2.0 * M_PI * ((float) i / (float) sides);
			glTexCoord2f(0.0, 1.0 * (float) i / (float) sides); 
			glVertex3f(cos(theta) * rad, sin(theta) * rad, zmin);
			glTexCoord2f(stretch, 1.0 * (float) i / (float) sides); 
			glVertex3f(cos(theta) * rad, sin(theta) * rad, zmax);
		} else {
			theta = 0.0;
			glTexCoord2f(0.0, 1.0);
			glVertex3f(cos(theta) * rad, sin(theta) * rad, zmin);
			glTexCoord2f(stretch, 1.0);
			glVertex3f(cos(theta) * rad, sin(theta) * rad, zmax);
		}
	}
	glEnd();
}

ENTRYPOINT void 
init_tunnel (ModeInfo *mi)
{
  int i;

  tunnel_configuration *tc;
  
  wire = MI_IS_WIREFRAME(mi);

# ifdef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
  wire = 0;
# endif

  if (!tconf) {
    tconf = (tunnel_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (tunnel_configuration));
    if (!tconf) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  tc = &tconf[MI_SCREEN(mi)];

  tc->glx_context = init_GL(mi);

  tc->cyllist = glGenLists(1);
  tc->diamondlist = glGenLists(1);
  tc->num_effects = 12;
  tc->num_texshifts = 3;
  tc->effect_time = 0.0;
  tc->effect_maxsecs = 30.00;
  /* check bounds on cmd line opts */
  if (start > tc->effect_maxsecs) start = tc->effect_maxsecs;
  if (end > tc->effect_maxsecs) end = tc->effect_maxsecs;
  if (start < tc->effect_time) start = tc->effect_time;
  if (end < tc->effect_time) end = tc->effect_time;

  /* set loop times, in seconds */
  tc->start_time = start;
  tc->end_time = end;

  /* reset animation knots, effect 0 not defined. */
  tc->effects = malloc(sizeof(effect_t) * ( tc->num_effects + 1));
  for ( i = 1; i <= tc->num_effects ; i++)
  	init_effects(&tc->effects[i], i);

  if (wire) {
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	do_texture = False;
  }

  if (do_texture)
  {
	  /* the following textures are loaded, and possible overridden:
		tunnel 1, tunnel 2, tunnel 3, marquee, tardis, head */
          glGenTextures(MAX_TEXTURE, tc->texture_binds);
	  
	  /*LoadTexture(*mi, **fn, *filename, texbind, bluralpha, bw_color,  anegative, onealpha)*/
  	  if (strcasecmp (do_tun1, "(none)")) /* tunnel 1 */
         	LoadTexture(mi, NULL, do_tun1, tc->texture_binds[0],  0,0.0, False, False);
	  else
          	LoadTexture(mi, timetunnel0_xpm, NULL, tc->texture_binds[0], 0, 0.0, False, False);
  	  if (strcasecmp (do_tun2, "(none)")) /* tunnel 2 */
         	LoadTexture(mi, NULL, do_tun2, tc->texture_binds[2],  0,0.0, False, False);
	  else
          	LoadTexture(mi, timetunnel1_xpm, NULL, tc->texture_binds[2], 0, 0.0, False, False);
  	  if (strcasecmp (do_tun3, "(none)")) /* tunnel 3 */
         	LoadTexture(mi, NULL, do_tun3, tc->texture_binds[5],  0,0.0, False, False);
	  else
          	LoadTexture(mi, timetunnel2_xpm, NULL, tc->texture_binds[5], 0, 0.0, False, False);
          LoadTexture(mi, tunnelstar_xpm, NULL, tc->texture_binds[4], 0, 0.0, False, False);
  	  if (strcasecmp (do_tx1, "(none)")) /* marquee */
         	LoadTexture(mi, NULL, do_tx1, tc->texture_binds[3],  0,0.0, False, False);
#ifndef HAVE_JWZGLES  /* logo_180_xpm is 180px which is not a power of 2! */
	  else
         	LoadTexture(mi, (char **) logo_180_xpm, NULL, tc->texture_binds[3],  0,0.0, False, False);
#endif
  	  if (strcasecmp (do_tx2, "(none)")) /* tardis */
         	LoadTexture(mi, NULL, do_tx2, tc->texture_binds[1], 0, 0.0 ,False, False);
#ifndef HAVE_JWZGLES  /* logo_180_xpm is 180px which is not a power of 2! */
	  else
         	LoadTexture(mi, (char **) logo_180_xpm, NULL, tc->texture_binds[1],  0,0.0, False, False);
#endif
  	  if (strcasecmp (do_tx3, "(none)")) { /* head */
         	LoadTexture(mi,  NULL, do_tx3, tc->texture_binds[6], 0, 0.0 ,False, False);
		/* negative */
          	LoadTexture(mi,  NULL, do_tx3, tc->texture_binds[9],  2,1.0, True, True);
#ifndef HAVE_JWZGLES  /* logo_180_xpm is 180px which is not a power of 2! */
	  } else {
         	LoadTexture(mi, (char **) logo_180_xpm, NULL, tc->texture_binds[6],  0,0.0, False, False);
		/* negative */
          	LoadTexture(mi, (char **) logo_180_xpm, NULL, tc->texture_binds[9],  2,1.0, True, True);
#endif
	  }
          glEnable(GL_TEXTURE_2D);
	  check_gl_error("tex");
  }

  reshape_tunnel (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glDisable(GL_DEPTH_TEST);  /* who needs it? ;-) */

  if (do_fog)
  	glEnable(GL_FOG);

  if (!wire)
    {
      glEnable(GL_ALPHA_TEST);
      glAlphaFunc(GL_GREATER, 0.5);
    }

    tc->trackball = gltrackball_init (True);


  tc->texshift = calloc(tc->num_texshifts, sizeof(GLfloat));
  for ( i = 0 ; i < tc->num_texshifts; i++)
  	tc->texshift[i] = 0.0;

  glNewList(tc->cyllist, GL_COMPILE);
  makecyl(30, -0.1, CYL_LEN, 1., 10. / 40.0 * CYL_LEN);  
  /*makecyl(30, -0.5, DIAMOND_LEN, 1., 4. / 40 * DIAMOND_LEN); */
  glEndList();

  glNewList(tc->diamondlist, GL_COMPILE);
  makecyl(4, -0.5, DIAMOND_LEN, 1., 4. / 40 * DIAMOND_LEN);
  glEndList();
}


ENTRYPOINT void
draw_tunnel (ModeInfo *mi)
{
  tunnel_configuration *tc = &tconf[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);


  if (!tc->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(tc->glx_context));

  glShadeModel(GL_SMOOTH);

  glEnable(GL_NORMALIZE);
  /* glEnable(GL_CULL_FACE); */

  glClear(GL_COLOR_BUFFER_BIT );

  update_animation(tc);


  glPushMatrix ();

	glRotatef(180., 0., 1., 0.);
    gltrackball_rotate (tc->trackball);
	glRotatef(180., 0., 1., 0.);



  mi->polygon_count = 0;

  update_fog(tc->effects[4].state[0],  /*color*/
	     tc->effects[4].state[1],  /*density*/
	     tc->effects[4].state[2],  /*start*/
	     tc->effects[4].state[3]); /*end*/

  /* --- begin composite image assembly --- */

  /* head mask and draw diamond tunnel */

  glEnable(GL_BLEND);
  draw_cyl(mi, tc, tc->effects[6].state[0], 5, tc->diamondlist, 2); 
  if (drawlogo)
  	draw_sign(mi, tc,tc->effects[12].state[0], tc->effects[12].state[1],  1.0 / 1.33, 9, 1); 
  glDisable(GL_BLEND);
  /* then tardis tunnel */
  make_wall_tunnel(mi, tc, tc->effects[1].state[0], tc->effects[7].state[0]);

  /* then cylinder tunnel */
  glEnable(GL_BLEND);
  draw_cyl(mi, tc, tc->effects[3].state[0], 2, tc->cyllist, 1); 

       /*void draw_sign(mi, tc,z,alpha,aspect,tex,blendmode)*/
  /* tardis */
  if (drawlogo)
  	draw_sign(mi, tc, tc->effects[2].state[0], tc->effects[2].state[1], 2.0, 1, 0);
  /* marquee */
  if (drawlogo)
  	draw_sign(mi, tc, tc->effects[5].state[0], tc->effects[5].state[1], 1.0, 3, 0);
  /*who head brite*/
  if (drawlogo)
  	draw_sign(mi, tc,1.0, tc->effects[10].state[0],  1.0 / 1.33, 6, 2);
  /*who head psychadelic REMOVED*/
  /* draw_sign(mi, tc,1.0, tc->effects[11].state[0],  1.0 / 1.33, 8, 0); */

  /* star */
  /* draw_sign(mi, tc, tc->effects[8].state[0]tc->effects[8].state[0], 1.0 , 1.0, 4, 1); */
  draw_sign(mi, tc,  tc->effects[8].state[0],  tc->effects[8].state[0],  1.0, 4, 1);
 
  /* normal head */
  if (drawlogo)
  	draw_sign(mi, tc,1.0, tc->effects[9].state[0], 1.0 /  1.33, 6, 0);

  /* --- end composite image assembly --- */


  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  check_gl_error("drawing done, calling swap buffers");
  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("TimeTunnel", timetunnel, tunnel)

#endif /* USE_GL */
