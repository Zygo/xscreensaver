/* -*- Mode: C; tab-width: 4 -*- */
/* Klein --- Klein Bottle, Moebius and other parametric surfaces
 * visualization */

/*
 * Revision History:
 * 2000: written by Andrey Mirtchovski <mirtchov@cpsc.ucalgary.ca
 *       
 * 01-Mar-2003  mirtchov    modified as a xscreensaver hack
 *
 */

#ifdef STANDALONE
# define DEFAULTS					"*delay:		20000   \n" \
									"*showFPS:      False   \n"

# define refresh_klein 0
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"			/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#define DEF_SPIN			        "True"
#define DEF_WANDER			        "False"
#define DEF_RANDOM			        "True"
#define DEF_SPEED			        "150"

#include "rotator.h"
#include "gltrackball.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

/* surfaces being drawn */
enum { 
	KLEIN = 0,
	DINI,
	ENNEPER,
	KUEN,
	MOEBIUS,
	SEASHELL,
	SWALLOWTAIL,
	BOHEM,
    SURFACE_LAST
};

/* primitives to draw with 
 * note that we skip the polygons and
 * triangle fans -- too slow
 *
 * also removed triangle_strip and quads -- 
 * just doesn't look good enough
 */
enum {
	MY_POINTS = 0,
	MY_LINES,
	MY_LINE_LOOP,
	MY_PRIM_LAST
};


static Bool rand;
static int render;
static int speed;
static Bool do_spin;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  {"-speed",   ".speed",    XrmoptionSepArg, 0 },
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-random", ".rand", XrmoptionNoArg, "True" },
  { "+random", ".rand", XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&rand,      "rand",   "Random", DEF_RANDOM, t_Bool},
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Int},
};


ENTRYPOINT ModeSpecOpt klein_opts = {countof(opts), opts, countof(vars), vars, NULL};



typedef struct{
  GLfloat x;
  GLfloat y;
  GLfloat z;
} GL_VECTOR;

typedef struct {
	GLXContext *glx_context;
	Window      window;
	rotator    *rot;
	trackball_state *trackball;
	Bool		  button_down_p;

	int render;
	int surface;

	float du, dv;
	float a, b, c;

    float draw_step;

} kleinstruct;

static kleinstruct *klein = NULL;


static void
draw(ModeInfo *mi)
{
	kleinstruct *kp = &klein[MI_SCREEN(mi)];
	double u, v;
	float coord[3];
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glEnable(GL_CULL_FACE);

	glPushMatrix();

	{
		double x, y, z;
		get_position (kp->rot, &x, &y, &z, !kp->button_down_p);
		glTranslatef((x - 0.5) * 10,
								 (y - 0.5) * 10,
								 (z - 0.5) * 20);

		gltrackball_rotate (kp->trackball);

		get_rotation (kp->rot, &x, &y, &z, !kp->button_down_p);
		glRotatef (x * 360, 1.0, 0.0, 0.0);
		glRotatef (y * 360, 0.0, 1.0, 0.0);
		glRotatef (z * 360, 0.0, 0.0, 1.0);
	}

	glScalef( 4.0, 4.0, 4.0 );

	glBegin(kp->render);
	switch(kp->surface) {
	case KLEIN:
		for(u = -M_PI; u < M_PI; u+=kp->du){
			for(v = -M_PI; v < M_PI; v+=kp->dv){
				coord[0] = cos(u)*(kp->a + sin(v)*cos(u/2) -
							sin(2*v)*sin(u/2)/2);
				coord[1] = sin(u)*(kp->a + sin(v)*cos(u/2) -
							sin(2*v)*sin(u/2)/2);
				coord[2] = sin(u/2)*sin(v) + cos(u/2)*sin(2*v)/2;
				glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
				glVertex3fv(coord);
			}
		}
		break;
		case DINI:
			for(u = -M_PI; u < M_PI; u+=kp->du){
				for(v = -M_PI; v < M_PI; v+=kp->dv){
					coord[0] = kp->a*cos(u)*sin(v);
					coord[1] = kp->a*sin(u)*sin(v);
					coord[2] = kp->a*(cos(v) + sin(tan((v/2))))+0.2*u;
					glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
					glVertex3fv(coord);
				}
			}
			break;
		case ENNEPER:
			for(u = -M_PI; u < M_PI; u+=kp->du){
				for(v = -M_PI; v < M_PI; v+=kp->dv){
					coord[0] = kp->a*(u-(u*u*u/3)+u*v*v);
					coord[1] = kp->b*(v-(v*v*v/3)+u*u*v);
					coord[2] = u*u-v*v;
					glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
					glVertex3fv(coord);
				}
			}
			break;
		case KUEN:
			for(u = -M_PI; u < M_PI; u+=kp->du){
				for(v = -M_PI; v < M_PI; v+=kp->dv){
					coord[0] = 2*(cos(u)+u*sin(u))*sin(v)/(1+u*u*sin(v)*sin(v));
					coord[1] = 2*(sin(u)-u*cos(u))*sin(v)/(1+u*u*sin(v)*sin(v));
					coord[2] = sin(tan(v/2))+2*cos(v)/(1+u*u*sin(v)*sin(v));

					glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
					glVertex3fv(coord);
				}
			}
			break;
		case MOEBIUS:
			for(u = -M_PI; u < M_PI; u+=kp->du){
				for(v = -M_PI; v < M_PI; v+=kp->dv){
					coord[0] = cos(u)+v*cos(u/2)*cos(u);
					coord[1] = sin(u)+v*cos(u/2)*sin(u);
					coord[2] = v*sin(u/2);
					glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
					glVertex3fv(coord);
				}
			}
			break;
		case SEASHELL:
			for(u = 0; u < 2*M_PI; u+=kp->du){
				for(v = 0; v < 2*M_PI; v+=kp->dv){
					coord[0] = kp->a*(1-v/(2*M_PI))*cos(2*v)*(1+cos(u))+sin(kp->c+=0.00001)*cos(2*v);
					coord[1] = kp->a*(1-v/(2*M_PI))*sin(2*v)*(1+cos(u))+cos(kp->c+=0.00001)*sin(2*v);
					coord[2] = sin(kp->b+=0.00001)*v/(2*M_PI)+kp->a*(1-v/(2*M_PI))*sin(u);
					glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
					glVertex3fv(coord);
				}
			}
			break;
		case SWALLOWTAIL:
			for(u = -M_PI; u < M_PI; u+=kp->du){
				for(v = -M_PI; v < M_PI; v+=kp->dv){
					coord[0] = u*pow(v,2) + 3*pow(v,4);
					coord[1] = -2*u*v - 4*pow(v,3);
					coord[2] = u;
					glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
					glVertex3fv(coord);
				}
			}
			break;
		case BOHEM:
			for(u = -M_PI; u < M_PI; u+=kp->du){
				for(v = -M_PI; v < M_PI; v+=kp->dv){
					coord[0] = kp->a*cos(u);
					coord[1] = 1.5*cos(v) + kp->a*sin(u);
					coord[2] = sin(v);
					glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
					glVertex3fv(coord);
				}
			}
			break;
		default:
			for(u = -M_PI; u < M_PI; u+=kp->du){
				for(v = -M_PI; v < M_PI; v+=kp->dv){
					coord[0] = sin(u)*kp->a;	
					coord[1] = cos(u)*kp->a;
					coord[2] = sin(u/2)*cos(v) + cos(u/2)*sin(v);
					glColor3f(coord[0]+0.7, coord[1]+0.7, coord[2]+0.7);
					glVertex3fv(coord);
				}
			}
			break;
	}
	glEnd();
	glPopMatrix();


	kp->a = sin(kp->draw_step+=0.01);
	kp->b = cos(kp->draw_step+=0.01);
}


/* new window size or exposure */
ENTRYPOINT void
reshape_klein(ModeInfo *mi, int width, int height)
{
	GLfloat h = (GLfloat) height / (GLfloat) width;

	glViewport(0, 0, (GLint) width, (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective (30.0, 1/h, 1.0, 100.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt( 0.0, 0.0, 30.0,
			 0.0, 0.0, 0.0,
			 0.0, 1.0, 0.0);
	
	glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
klein_handle_event (ModeInfo *mi, XEvent *event)
{
	kleinstruct *kp = &klein[MI_SCREEN(mi)];

	if (event->xany.type == ButtonPress && event->xbutton.button == Button1) {
			kp->button_down_p = True;
			gltrackball_start (kp->trackball, event->xbutton.x, event->xbutton.y, MI_WIDTH (mi), MI_HEIGHT (mi));
			return True;
	} else if (event->xany.type == ButtonRelease && event->xbutton.button == Button1) {
			kp->button_down_p = False;
			return True;
	} else if (event->xany.type == ButtonPress &&
               (event->xbutton.button == Button4 ||
                event->xbutton.button == Button5)) {
      gltrackball_mousewheel (kp->trackball, event->xbutton.button, 10,
                              !!event->xbutton.state);
      return True;
    } else if (event->xany.type == MotionNotify && kp->button_down_p) {
			gltrackball_track (kp->trackball, event->xmotion.x, event->xmotion.y, MI_WIDTH (mi), MI_HEIGHT (mi));
			return True;
	}

	return False;
}


ENTRYPOINT void
init_klein(ModeInfo *mi)
{
	int	 screen = MI_SCREEN(mi);
	kleinstruct *kp;

	if (klein == NULL) {
		if ((klein = (kleinstruct *) calloc(MI_NUM_SCREENS(mi), sizeof (kleinstruct))) == NULL)
			return;
	}
	kp = &klein[screen];

	kp->window = MI_WINDOW(mi);

	{
		double spin_speed	 = 1.0;
		double wander_speed = 0.03;
		kp->rot = make_rotator (do_spin ? spin_speed : 0,
						do_spin ? spin_speed : 0,
						do_spin ? spin_speed : 0,
						1.0,
						do_wander ? wander_speed : 0,
						True);
		kp->trackball = gltrackball_init ();
	}

	if(rand) {
		render = random() % MY_PRIM_LAST;
		kp->surface = random() % SURFACE_LAST;
	} else {
		render = MY_LINE_LOOP;
		kp->surface = KLEIN;
	}

	switch (render) {
	case MY_POINTS: kp->render = GL_POINTS; break;
	case MY_LINES: kp->render = GL_LINES; break;
	case MY_LINE_LOOP: kp->render = GL_LINE_LOOP; break;
	default:
			kp->render = GL_LINE_LOOP;
	}
/*kp->render=GL_TRIANGLE_FAN;*/
/*kp->render=GL_POLYGON;*/

	kp->du = 0.07;
	kp->dv = 0.07;
	kp->a = kp->b = 1;
	kp->c = 0.1;


	if ((kp->glx_context = init_GL(mi)) != NULL) {
		reshape_klein(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
	} else {
		MI_CLEARWINDOW(mi);
	}
}

ENTRYPOINT void
draw_klein(ModeInfo * mi)
{
	kleinstruct *kp = &klein[MI_SCREEN(mi)];
	Display	*display = MI_DISPLAY(mi);
	Window	window = MI_WINDOW(mi);

	if (!kp->glx_context) return;

	glDrawBuffer(GL_BACK);

	glXMakeCurrent(display, window, *(kp->glx_context));
	draw(mi);
	if (mi->fps_p) do_fps (mi);
	glFinish();
	glXSwapBuffers(display, window);
}

ENTRYPOINT void
release_klein(ModeInfo * mi)
{
	if (klein != NULL) {
		int	 screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			kleinstruct *kp = &klein[screen];

			if (kp->glx_context) {
				/* Display lists MUST be freed while their glXContext is current. */
				glXMakeCurrent(MI_DISPLAY(mi), kp->window, *(kp->glx_context));
			}
		}
		(void) free((void *) klein);
		klein = NULL;
	}
	FreeAllGL(mi);
}


XSCREENSAVER_MODULE ("Klein", klein)

/*********************************************************/

#endif
