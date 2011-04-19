/* boxed --- 3D bouncing balls that explode */

#if 0
static const char sccsid[] = "@(#)boxed.c	0.9 01/09/26 xlockmore";
#endif

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
 * Revision History:
 *
 * 2001: Written by Sander van Grieken <mailsander@gmx.net> 
 *       as an OpenGL screensaver for the xscreensaver package.
 *       Lots of hardcoded values still in place. Also, there are some
 *       copy/paste leftovers from the gears hack. opts don't work.
 *
 * 2005: opts work. added options -balls, -ballsize, -explosion
 *
 * 2006: opts work. added option -decay
 *
 * 2008: opts work. added option -momentum
 *
 */

#include "boxed.h"

/*
**----------------------------------------------------------------------------
** Defines
**----------------------------------------------------------------------------
*/

#ifdef STANDALONE
# define DEFAULTS	"*delay:     20000   \n" \
			"*showFPS:   False   \n" \
			"*wireframe: False   \n"

# define refresh_boxed 0
# define boxed_handle_event 0
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

# define DEF_SPEED      "0.5"
# define DEF_BALLS	"25"
# define DEF_BALLSIZE   "2.0"
# define DEF_EXPLOSION	"15.0"
# define DEF_DECAY	"0.07"
# define DEF_MOMENTUM	"0.6"

#undef countof 
#define countof(x) (int)(sizeof((x))/sizeof((*x)))
#undef rnd
#define rnd() (frand(1.0))

static GLfloat speed;  /* jwz -- overall speed factor applied to all motion */
static int cfg_balls;
static GLfloat cfg_ballsize;
static GLfloat cfg_explosion;
static GLfloat cfg_decay;
static GLfloat cfg_momentum;


static XrmOptionDescRec opts[] = {
    {"-speed", ".boxed.speed", XrmoptionSepArg, 0},
    {"-balls", ".boxed.balls", XrmoptionSepArg, 0},
    {"-ballsize", ".boxed.ballsize", XrmoptionSepArg, 0},
    {"-explosion", ".boxed.explosion", XrmoptionSepArg, 0},
    {"-decay", ".boxed.decay", XrmoptionSepArg, 0},
    {"-momentum", ".boxed.momentum", XrmoptionSepArg, 0},
};

static argtype vars[] = {
    {&speed, "speed", "Speed", DEF_SPEED, t_Float},
    {&cfg_balls, "balls", "Balls", DEF_BALLS, t_Int},
    {&cfg_ballsize, "ballsize", "Ball Size", DEF_BALLSIZE, t_Float},
    {&cfg_explosion, "explosion", "Explosion", DEF_EXPLOSION, t_Float},
    {&cfg_decay, "decay", "Explosion Decay", DEF_DECAY, t_Float},
    {&cfg_momentum, "momentum", "Explosion Momentum", DEF_MOMENTUM, t_Float},
};

ENTRYPOINT ModeSpecOpt boxed_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES

ModStruct   boxed_description = { 
     "boxed", "init_boxed", "draw_boxed", "release_boxed",
     "draw_boxed", "init_boxed", NULL, &boxed_opts,
     1000, 1, 2, 1, 4, 1.0, "",
     "Shows GL's boxed balls", 0, NULL};

#endif

#define BOOL int
#define TRUE 1
#define FALSE 0

/* camera */
#define CAM_HEIGHT 	100.0f
#define CAMDISTANCE_MIN 20.0
#define CAMDISTANCE_MAX 150.0
#define CAMDISTANCE_SPEED 1.5

/* rendering the sphere */
#define MESH_SIZE	10
#define SPHERE_VERTICES	(2+MESH_SIZE*MESH_SIZE*2)
#define SPHERE_INDICES	((MESH_SIZE*4 + MESH_SIZE*4*(MESH_SIZE-1))*3)

/*
**-----------------------------------------------------------------------------
**	Typedefs
**-----------------------------------------------------------------------------
*/

typedef struct {
   GLfloat x;
   GLfloat y;
   GLfloat z;
} vectorf;

typedef struct {
   vectorf	loc;
   vectorf	dir;
   vectorf	color;
   float	radius;
   BOOL         bounced;
   int		offside;
   BOOL		justcreated;
} ball;

typedef struct {
   int		num_balls;
   float	ballsize;
   float	explosion;
   ball	        *balls;
} ballman;

typedef struct {
   vectorf	loc;
   vectorf	dir;
   BOOL         far;
   BOOL         gone;
} tri;

typedef struct {
   int		num_tri;
   int 		lifetime;
   float	scalefac;
   float	explosion;
   float	decay;
   float	momentum;
   vectorf	color;
   tri		*tris;
   GLint	*indices;
   vectorf      *normals;
   vectorf	*vertices;
} triman;

typedef struct {
   int numballs;
   float ballsize;
   float explosion;
   float decay;
   float momentum;
   BOOL textures;
   BOOL transparent;
   float camspeed;
} boxed_config;


typedef struct {
   float          cam_x_speed, cam_z_speed, cam_y_speed;
   boxed_config  config;
   float	  tic;
   float          camtic;
   vectorf        spherev[SPHERE_VERTICES];
   GLint          spherei[SPHERE_INDICES];
   ballman        bman;
   triman         *tman;
   GLXContext     *glx_context;
   GLuint         listobjects;
   GLuint         gllists[3];
   int            list_polys[3];
   Window         window;
   BOOL           stop;
   char           *tex1;
} boxedstruct;

#define GLL_PATTERN 0
#define GLL_BALL    1
#define GLL_BOX     2

/*
**----------------------------------------------------------------------------
** Local Variables
**----------------------------------------------------------------------------
*/

static boxedstruct *boxed = NULL;


/*
**----------------------------------------------------------------------------
** Functions
**----------------------------------------------------------------------------
*/

/*
 * Add 2 vectors
 */ 
static inline void addvectors(vectorf *dest, vectorf *s1, vectorf *s2) 
{
   dest->x = s1->x + s2->x;
   dest->y = s1->y + s2->y;
   dest->z = s1->z + s2->z;
}

/*
 * Sub 2 vectors
 */ 
static inline void subvectors(vectorf *dest, vectorf* s1, vectorf *s2) 
{
   dest->x = s1->x - s2->x;
   dest->y = s1->y - s2->y;
   dest->z = s1->z - s2->z;
}

/*
 * Multiply vector with scalar (scale vector)
 */ 
static inline void scalevector(vectorf *dest, vectorf *source, GLfloat sc)
{
   dest->x = source->x * sc;
   dest->y = source->y * sc;
   dest->z = source->z * sc;
}

/*
 * Copy vector
 */
static inline void copyvector(vectorf *dest, vectorf* source) 
{
   dest->x = source->x;
   dest->y = source->y;
   dest->z = source->z;
}


static inline GLfloat
dotproduct(vectorf * v1, vectorf * v2)
{
   return v1->x * v2->x + v1->y * v2->y + v1->z * v2->z;
}

static inline GLfloat
squaremagnitude(vectorf * v)
{
   return v->x * v->x + v->y * v->y + v->z * v->z;
}

static inline GLfloat
squaremagnitudehorz(vectorf * v)
{
   return v->x * v->x + v->z * v->z;
}



/*
 * Generate the Sphere data
 * 
 * Input: 
 */ 

static void generatesphere(boxedstruct *gp)
{
   float   dj = M_PI/(MESH_SIZE+1.0f);
   float   di = M_PI/MESH_SIZE;
   int	   v;	/* vertex offset */
   int     ind;	/* indices offset */
   int     i,j,si;
   GLfloat r_y_plane, h_y_plane;
   vectorf *spherev;
   GLint   *spherei;
   
   /*
    * generate the sphere data
    * vertices 0 and 1 are the north and south poles
    */
   
   spherei = gp->spherei;
   spherev = gp->spherev;
   
   spherev[0].x = 0.0f; spherev[0].y =1.0f; spherev[0].z = 0.0f;
   spherev[1].x = 0.0f; spherev[1].y =-1.0f; spherev[1].z = 0.0f;
   
   for (j=0; j<MESH_SIZE; j++) {
      r_y_plane = (float)sin((j+1) * dj);
      h_y_plane = (float)cos((j+1) * dj);
      for (i=0; i<MESH_SIZE*2; i++) {
	 si = 2+i+j*MESH_SIZE*2;
	 spherev[si].y = h_y_plane;
	 spherev[si].x = (float) sin(i * di) * r_y_plane;
	 spherev[si].z = (float) cos(i * di) * r_y_plane;
      }
   }
   
   /* generate indices */
   for (i=0; i<MESH_SIZE*2; i++) {
      spherei[3*i] = 0;
      spherei[3*i+1] = i+2;
      spherei[3*i+2] = i+3;
      if (i==MESH_SIZE*2-1)
	spherei[3*i+2] = 2;
   }
   
   /* the middle strips */
   for (j=0; j<MESH_SIZE-1; j++) {
      v = 2+j*MESH_SIZE*2;
      ind = 3*MESH_SIZE*2 + j*6*MESH_SIZE*2;
      for (i=0; i<MESH_SIZE*2; i++) {
	 spherei[6*i+ind] = v+i;
	 spherei[6*i+2+ind] = v+i+1;
	 spherei[6*i+1+ind] = v+i+MESH_SIZE*2;
	 
	 spherei[6*i+ind+3] = v+i+MESH_SIZE*2;
	 spherei[6*i+2+ind+3] = v+i+1;
	 spherei[6*i+1+ind+3] = v+i+MESH_SIZE*2+1;
	 if (i==MESH_SIZE*2-1) {
	    spherei[6*i+2+ind] = v+i+1-2*MESH_SIZE;
	    spherei[6*i+2+ind+3] = v+i+1-2*MESH_SIZE;
	    spherei[6*i+1+ind+3] = v+i+MESH_SIZE*2+1-2*MESH_SIZE;
	 }
      }
   }
   
   v = SPHERE_VERTICES-MESH_SIZE*2;
   ind = SPHERE_INDICES-3*MESH_SIZE*2;
   for (i=0; i<MESH_SIZE*2; i++) {
      spherei[3*i+ind] = 1;
      spherei[3*i+1+ind] = v+i+1;
      spherei[3*i+2+ind] = v+i;
      if (i==MESH_SIZE*2-1)
	spherei[3*i+1+ind] = v;
   }
}

      
      
    
/*
 * create fresh ball 
 */

static void createball(ball *newball) 
{
   float r=0.0f,g=0.0f,b=0.0f;
   newball->loc.x = 5-10*rnd();
   newball->loc.y = 35+20*rnd();
   newball->loc.z = 5-10*rnd();
   newball->dir.x = (0.5f-rnd()) * speed;
   newball->dir.y = 0.0;
   newball->dir.z = (0.5-rnd())  * speed;
   newball->offside = 0;
   newball->bounced = FALSE;
   newball->radius = cfg_ballsize;
   while (r+g+b < 1.8f ) {
      newball->color.x = r=rnd();
      newball->color.y = g=rnd();
      newball->color.z = b=rnd();
   }
   newball->justcreated = TRUE;
}

/* Update position of each ball */

static void updateballs(ballman *bman) 
{
   register int b,j;
   vectorf dvect,richting,relspeed,influence;
   GLfloat squaredist;

   for (b=0;b<bman->num_balls;b++) {

     GLfloat gravity = 0.30f * speed;

      /* apply gravity */
      bman->balls[b].dir.y -= gravity;
      /* apply movement */
      addvectors(&bman->balls[b].loc,&bman->balls[b].loc,&bman->balls[b].dir);
      /* boundary check */
      if (bman->balls[b].loc.y < bman->balls[b].radius) { /* ball onder bodem? (bodem @ y=0) */
	 if ((bman->balls[b].loc.x < -95.0) || 
	     (bman->balls[b].loc.x > 95.0) ||
	     (bman->balls[b].loc.z < -95.0) ||
	     (bman->balls[b].loc.z > 95.0)) {
	    if (bman->balls[b].loc.y < -1000.0)
	      createball(&bman->balls[b]);
	 } else {
	    bman->balls[b].loc.y = bman->balls[b].radius  + (bman->balls[b].radius - bman->balls[b].loc.y);
	    bman->balls[b].dir.y = -bman->balls[b].dir.y;
	    if (bman->balls[b].offside) {
	       bman->balls[b].bounced = TRUE; /* temporary disable painting ball */
	       scalevector(&bman->balls[b].dir,&bman->balls[b].dir,0.80f);
	       if (squaremagnitude(&bman->balls[b].dir) < 0.08f) {
		  createball(&bman->balls[b]);
               }
	       if (squaremagnitudehorz(&bman->balls[b].dir) < 0.005f) {
                 createball(&bman->balls[b]);
               }
	    }
	 }
	 
      }
      if (!bman->balls[b].offside) {
	 if (bman->balls[b].loc.x - bman->balls[b].radius < -20.0f) { /* x ondergrens */
	    if (bman->balls[b].loc.y > 41+bman->balls[b].radius)  bman->balls[b].offside=1;
	    else {
	       bman->balls[b].dir.x = -bman->balls[b].dir.x;
	       bman->balls[b].loc.x = -20.0f + bman->balls[b].radius;
	    }
	 }
	 if (bman->balls[b].loc.x + bman->balls[b].radius > 20.0f) { /* x bovengrens */
	    if (bman->balls[b].loc.y > 41+bman->balls[b].radius)  bman->balls[b].offside=1;
	    else {
	       bman->balls[b].dir.x = -bman->balls[b].dir.x;
	       bman->balls[b].loc.x = 20.0f - bman->balls[b].radius;
	    }
	 }
	 if (bman->balls[b].loc.z - bman->balls[b].radius < -20.0f) { /* z ondergrens */
	    if (bman->balls[b].loc.y > 41+bman->balls[b].radius)  bman->balls[b].offside=1;
	    else {
	       bman->balls[b].dir.z = -bman->balls[b].dir.z;
	       bman->balls[b].loc.z = -20.0f + bman->balls[b].radius;
	    }
	 }
	 if (bman->balls[b].loc.z + bman->balls[b].radius > 20.0f) { /* z bovengrens */
	    if (bman->balls[b].loc.y > 41+bman->balls[b].radius)  bman->balls[b].offside=1;
	    else {
	       bman->balls[b].dir.z = -bman->balls[b].dir.z;
	       bman->balls[b].loc.z = 20.0f - bman->balls[b].radius;
	    }
	 }
      } /* end if !offside */
   
      /* check voor stuiteren */
      for (j=b+1;j<bman->num_balls;j++) {
	 squaredist = (bman->balls[b].radius * bman->balls[b].radius) + (bman->balls[j].radius * bman->balls[j].radius);
	 subvectors(&dvect,&bman->balls[b].loc,&bman->balls[j].loc);
	 if ( squaremagnitude(&dvect) < squaredist ) { /* balls b and j touch */
	    subvectors(&richting,&bman->balls[j].loc,&bman->balls[b].loc);
	    subvectors(&relspeed,&bman->balls[b].dir,&bman->balls[j].dir);
	    /* calc mutual influence direction and magnitude */
	    scalevector(&influence,&richting,(dotproduct(&richting,&relspeed)/squaremagnitude(&richting)));
	    
	    subvectors(&bman->balls[b].dir,&bman->balls[b].dir,&influence);
	    addvectors(&bman->balls[j].dir,&bman->balls[j].dir,&influence);
	    addvectors(&bman->balls[b].loc,&bman->balls[b].loc,&bman->balls[b].dir);
	    addvectors(&bman->balls[j].loc,&bman->balls[j].loc,&bman->balls[j].dir);
	    
	    subvectors(&dvect,&bman->balls[b].loc,&bman->balls[j].loc);
	    while (squaremagnitude(&dvect) < squaredist) {
	       addvectors(&bman->balls[b].loc,&bman->balls[b].loc,&bman->balls[b].dir);
	       addvectors(&bman->balls[j].loc,&bman->balls[j].loc,&bman->balls[j].dir);
	       subvectors(&dvect,&bman->balls[b].loc,&bman->balls[j].loc);
	    }
	 }
      } /* end for j */
   } /* end for b */
}


/*
* explode ball into triangles
*/

static void createtrisfromball(triman* tman, vectorf *spherev, GLint *spherei, int ind_num, ball *b)
{
   int pos;
   float explosion;
   float momentum;
   float scale;
   register int i;
   vectorf avgdir,dvect,mvect;

   tman->scalefac = b->radius;
   copyvector(&tman->color,&b->color);
   explosion = 1.0f + tman->explosion * 2.0 * rnd();
   momentum = tman->momentum;

   tman->num_tri = ind_num/3;
   
   /* reserveer geheugen voor de poly's in een bal */
   
   tman->tris = (tri *)malloc(tman->num_tri * sizeof(tri));
   tman->vertices = (vectorf *)malloc(ind_num * sizeof(vectorf));
   tman->normals = (vectorf *)malloc(ind_num/3 * sizeof(vectorf));
   
   for (i=0; i<(tman->num_tri); i++) {
      tman->tris[i].far = FALSE;
      tman->tris[i].gone = FALSE;
      pos = i * 3;
      /* kopieer elke poly apart naar een tri structure */
      copyvector(&tman->vertices[pos+0],&spherev[spherei[pos+0]]);
      copyvector(&tman->vertices[pos+1],&spherev[spherei[pos+1]]);
      copyvector(&tman->vertices[pos+2],&spherev[spherei[pos+2]]);
      /* Calculate average direction of shrapnel */
      addvectors(&avgdir,&tman->vertices[pos+0],&tman->vertices[pos+1]);
      addvectors(&avgdir,&avgdir,&tman->vertices[pos+2]);
      scalevector(&avgdir,&avgdir,0.33333);
      
      /* should normalize first, NYI */
      copyvector(&tman->normals[i],&avgdir);
     
      /* copy de lokatie */
      addvectors(&tman->tris[i].loc,&b->loc,&avgdir);
      /* en translate alle triangles terug naar hun eigen oorsprong */
      tman->vertices[pos+0].x -= avgdir.x;
      tman->vertices[pos+0].y -= avgdir.y;
      tman->vertices[pos+0].z -= avgdir.z;
      tman->vertices[pos+1].x -= avgdir.x;
      tman->vertices[pos+1].y -= avgdir.y;
      tman->vertices[pos+1].z -= avgdir.z;
      tman->vertices[pos+2].x -= avgdir.x;
      tman->vertices[pos+2].y -= avgdir.y;
      tman->vertices[pos+2].z -= avgdir.z;
      /* alwaar opschaling plaatsvindt */
      scale = b->radius * 2;
      scalevector(&tman->vertices[pos+0],&tman->vertices[pos+0],scale);
      scalevector(&tman->vertices[pos+1],&tman->vertices[pos+1],scale);
      scalevector(&tman->vertices[pos+2],&tman->vertices[pos+2],scale);
            
      tman->vertices[pos+0].x += avgdir.x;
      tman->vertices[pos+0].y += avgdir.y;
      tman->vertices[pos+0].z += avgdir.z;
      tman->vertices[pos+1].x += avgdir.x;
      tman->vertices[pos+1].y += avgdir.y;
      tman->vertices[pos+1].z += avgdir.z;
      tman->vertices[pos+2].x += avgdir.x;
      tman->vertices[pos+2].y += avgdir.y;
      tman->vertices[pos+2].z += avgdir.z;

      /* bereken nieuwe richting */
      scalevector(&tman->tris[i].dir,&avgdir,explosion);
      dvect.x = (0.1f - 0.2f*rnd());
      dvect.y = (0.15f - 0.3f*rnd());
      dvect.z = (0.1f - 0.2f*rnd());
      addvectors(&tman->tris[i].dir,&tman->tris[i].dir,&dvect);

      /* add ball's momentum to each piece of the exploded ball */
      mvect.x = b->dir.x * momentum;
      mvect.y = 0;
      mvect.z = b->dir.z * momentum;
      addvectors(&tman->tris[i].dir,&tman->tris[i].dir,&mvect);
   }
}


/*
* update position of each tri
*/

static void updatetris(triman *t) 
{
   int b;
   GLfloat xd,zd;
   
   for (b=0;b<t->num_tri;b++) {
      /* the exploded triangles disappear over time */
      if (rnd() < t->decay) { t->tris[b].gone = TRUE; }
      /* apply gravity */
      t->tris[b].dir.y -= (0.1f * speed);
      /* apply movement */
      addvectors(&t->tris[b].loc,&t->tris[b].loc,&t->tris[b].dir);
      /* boundary check */
      if (t->tris[b].far) continue;
      if (t->tris[b].loc.y < 0) { /* onder bodem ? */
	 if ((t->tris[b].loc.x > -95.0f) &
	     (t->tris[b].loc.x < 95.0f) &
	     (t->tris[b].loc.z > -95.0f) &
	     (t->tris[b].loc.z < 95.0f)) {  /* in veld  */
	    t->tris[b].dir.y = -(t->tris[b].dir.y);
	    t->tris[b].loc.y = -t->tris[b].loc.y;
	    scalevector(&t->tris[b].dir,&t->tris[b].dir,0.80f); /* dampening */
	 }
	 else {
	    t->tris[b].far = TRUE;
	    continue;
	 }
      }
      
      if ((t->tris[b].loc.x > -21.0f) &
	  (t->tris[b].loc.x < 21.0f) &
	  (t->tris[b].loc.z > -21.0f) &
	  (t->tris[b].loc.z < 21.0f)) { /* in box? */

	 xd = zd = 999.0f; /* big */
	 if ((t->tris[b].loc.x > -21.0f) &
	     (t->tris[b].loc.x < 0)) {
	    xd = t->tris[b].loc.x + 21.0f;
	 }
	 if ((t->tris[b].loc.x < 21.0f) &
	     (t->tris[b].loc.x > 0)) {
	    xd = 21.0f - t->tris[b].loc.x;
	 }
	 if ((t->tris[b].loc.z > -21.0f) &
	     (t->tris[b].loc.z < 0)) {
	    zd = t->tris[b].loc.z + 21.0f;
	 }
	 if ((t->tris[b].loc.z < 21.0f) &
	     (t->tris[b].loc.z > 0)) {
	    zd = 21.0f - t->tris[b].loc.z;
	 }
	 if (xd < zd) {
	    /* bounce x */
	    if (t->tris[b].dir.x < 0) 
	      t->tris[b].loc.x += (21.0f - t->tris[b].loc.x);
	    else
	      t->tris[b].loc.x += (-21.0f - t->tris[b].loc.x);
	    t->tris[b].dir.x = -t->tris[b].dir.x;
	 } else { 
	    /* bounce z */
	    if (t->tris[b].dir.z < 0)
	      t->tris[b].loc.z += (21.0f - t->tris[b].loc.z);
	    else
	      t->tris[b].loc.z += (-21.0f - t->tris[b].loc.z);
	    t->tris[b].dir.z = -t->tris[b].dir.z;
	 }	 
	       
      }
   } /* end for b */
} 

	     
/*
 * free memory allocated by a tri manager
 */
static void freetris(triman *t) 
{
   if (!t) return;
   if (t->tris) free(t->tris);
   if (t->vertices) free(t->vertices);
   if (t->normals) free(t->normals);
   t->tris = NULL;
   t->vertices = NULL;
   t->normals = NULL;
   t->num_tri = 0;
   t->lifetime = 0;
}


/*
 *load defaults in config structure
 */
static void setdefaultconfig(boxed_config *config) 
{
  cfg_balls = MAX(3,MIN(40,cfg_balls));
  cfg_ballsize = MAX(1.0f,MIN(5.0f,cfg_ballsize));
  cfg_explosion = MAX(0.0f,MIN(50.0f,cfg_explosion));
  cfg_decay = MAX(0.0f,MIN(1.0f,cfg_decay));
  cfg_momentum = MAX(0.0f,MIN(1.0f,cfg_momentum));

  config->numballs = cfg_balls;
  config->textures = TRUE;
  config->transparent = FALSE;
  config->explosion = cfg_explosion;
  config->decay = cfg_decay;
  config->momentum = cfg_momentum;
  config->ballsize = cfg_ballsize;
  config->camspeed = 35.0f;
}


/*
 * draw bottom
 */ 
static int drawfilledbox(boxedstruct *boxed, int wire)
{   
   /* draws texture filled box, 
      top is drawn using the entire texture, 
      the sides are drawn using the edge of the texture
    */
  int polys = 0;
   
   /* front */
   glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
   glTexCoord2f(0,1);
   glVertex3f(-1.0,1.0,1.0);
   glTexCoord2f(1,1);
   glVertex3f(1.0,1.0,1.0);
   glTexCoord2f(1,1);
   glVertex3f(1.0,-1.0,1.0);
   glTexCoord2f(0,1);
   glVertex3f(-1.0,-1.0,1.0);
   polys++;
   /* rear */
   glTexCoord2f(0,1);
   glVertex3f(1.0,1.0,-1.0);
   glTexCoord2f(1,1);
   glVertex3f(-1.0,1.0,-1.0);
   glTexCoord2f(1,1);
   glVertex3f(-1.0,-1.0,-1.0);
   glTexCoord2f(0,1);
   glVertex3f(1.0,-1.0,-1.0);
   polys++;
   /* left */
   glTexCoord2f(1,1);
   glVertex3f(-1.0,1.0,1.0);
   glTexCoord2f(1,1);
   glVertex3f(-1.0,-1.0,1.0);
   glTexCoord2f(0,1);
   glVertex3f(-1.0,-1.0,-1.0);
   glTexCoord2f(0,1);
   glVertex3f(-1.0,1.0,-1.0);
   polys++;
   /* right */
   glTexCoord2f(0,1);
   glVertex3f(1.0,1.0,1.0);
   glTexCoord2f(1,1);
   glVertex3f(1.0,1.0,-1.0);
   glTexCoord2f(1,1);
   glVertex3f(1.0,-1.0,-1.0);
   glTexCoord2f(0,1);
   glVertex3f(1.0,-1.0,1.0);
   polys++;
   /* top */
   glTexCoord2f(0.0,0.0);
   glVertex3f(-1.0,1.0,1.0);
   glTexCoord2f(0.0,1.0);
   glVertex3f(-1.0,1.0,-1.0);
   glTexCoord2f(1.0,1.0);
   glVertex3f(1.0,1.0,-1.0);
   glTexCoord2f(1.0,0.0);
   glVertex3f(1.0,1.0,1.0);
   polys++;
   /* bottom */
   glTexCoord2f(0,0);
   glVertex3f(-1.0,-1.0,1.0);
   glTexCoord2f(0,1);
   glVertex3f(-1.0,-1.0,-1.0);
   glTexCoord2f(1,1);
   glVertex3f(1.0,-1.0,-1.0);
   glTexCoord2f(1,0);
   glVertex3f(1.0,-1.0,1.0);
   polys++;
   glEnd();

   return polys;
}


/*
 * Draw a box made of lines
 */ 
static int drawbox(boxedstruct *boxed)
{
   int polys = 0;
   /* top */
   glBegin(GL_LINE_STRIP);
   glVertex3f(-1.0,1.0,1.0);
   glVertex3f(-1.0,1.0,-1.0); polys++;
   glVertex3f(1.0,1.0,-1.0); polys++;
   glVertex3f(1.0,1.0,1.0); polys++;
   glVertex3f(-1.0,1.0,1.0); polys++;
   glEnd();
   /* bottom */
   glBegin(GL_LINE_STRIP);
   glVertex3f(-1.0,-1.0,1.0);
   glVertex3f(1.0,-1.0,1.0); polys++;
   glVertex3f(1.0,-1.0,-1.0); polys++;
   glVertex3f(-1.0,-1.0,-1.0); polys++;
   glVertex3f(-1.0,-1.0,1.0); polys++;
   glEnd();
   /* connect top & bottom */
   glBegin(GL_LINES);
   glVertex3f(-1.0,1.0,1.0);
   glVertex3f(-1.0,-1.0,1.0); polys++;
   glVertex3f(1.0,1.0,1.0);
   glVertex3f(1.0,-1.0,1.0); polys++;
   glVertex3f(1.0,1.0,-1.0);
   glVertex3f(1.0,-1.0,-1.0); polys++;
   glVertex3f(-1.0,1.0,-1.0);
   glVertex3f(-1.0,-1.0,-1.0); polys++;
   glEnd();
   return polys;
}


  
/* 
 * Draw ball
 */
static int drawball(boxedstruct *gp, ball *b, int wire)
{
   int polys = 0;
   int i,pos,cnt;
   GLint *spherei = gp->spherei;
   vectorf *spherev = gp->spherev;
   GLfloat col[3];
   
   glPushMatrix();
   
   glTranslatef(b->loc.x,b->loc.y,b->loc.z);
   glScalef(b->radius,b->radius,b->radius);
   glColor3f(b->color.x,b->color.y,b->color.z);
   col[0] = b->color.x;
   col[1] = b->color.y;
   col[2] = b->color.z;
   glMaterialfv(GL_FRONT, GL_DIFFUSE, col);
   col[0] *= 0.5;
   col[1] *= 0.5;
   col[2] *= 0.5;
   glMaterialfv(GL_FRONT, GL_EMISSION,col);

   if (!gp->gllists[GLL_BALL]) {
      glNewList(gp->listobjects + GLL_BALL,GL_COMPILE);
      cnt = SPHERE_INDICES/3;
      for (i=0; i<cnt; i++) {
	 pos = i * 3;
	 glBegin(wire ? GL_LINE_LOOP : GL_TRIANGLES);
	 glNormal3f(spherev[spherei[pos+0]].x,spherev[spherei[pos+0]].y,spherev[spherei[pos+0]].z);
	 glVertex3f(spherev[spherei[pos+0]].x,spherev[spherei[pos+0]].y,spherev[spherei[pos+0]].z);
	 glNormal3f(spherev[spherei[pos+1]].x,spherev[spherei[pos+1]].y,spherev[spherei[pos+1]].z);
         gp->list_polys[GLL_BALL]++;
	 glVertex3f(spherev[spherei[pos+1]].x,spherev[spherei[pos+1]].y,spherev[spherei[pos+1]].z);
	 glNormal3f(spherev[spherei[pos+2]].x,spherev[spherei[pos+2]].y,spherev[spherei[pos+2]].z);
	 glVertex3f(spherev[spherei[pos+2]].x,spherev[spherei[pos+2]].y,spherev[spherei[pos+2]].z);
         gp->list_polys[GLL_BALL]++;
	 glEnd();
      }
      glEndList();
      gp->gllists[GLL_BALL] = 1;
   } else {
      glCallList(gp->listobjects + GLL_BALL);
      polys += gp->list_polys[GLL_BALL];
   }
   
   glPopMatrix();
   return polys;
}

    
/* 
 * Draw all triangles in triman
 */
static int drawtriman(triman *t, int wire) 
{
   int polys = 0;
   int i,pos;
   vectorf *spherev = t->vertices;
   GLfloat col[3];
   
   glPushMatrix();
   glColor3f(t->color.x,t->color.y,t->color.z);
   col[0] = t->color.x;
   col[1] = t->color.y;
   col[2] = t->color.z;
   glMaterialfv(GL_FRONT, GL_DIFFUSE, col);
   col[0] *= 0.3;
   col[1] *= 0.3;
   col[2] *= 0.3;
   glMaterialfv(GL_FRONT, GL_EMISSION,col);
   
   for (i=0; i<t->num_tri; i++) {
      if (t->tris[i].gone == TRUE) { continue; }
      pos = i*3;
      glPushMatrix();
      glTranslatef(t->tris[i].loc.x,t->tris[i].loc.y,t->tris[i].loc.z);
      glBegin(wire ? GL_LINE_LOOP : GL_TRIANGLES);
      glNormal3f(t->normals[i].x,t->normals[i].y,t->normals[i].z);
      glVertex3f(spherev[pos+0].x,spherev[pos+0].y,spherev[pos+0].z);
      glVertex3f(spherev[pos+1].x,spherev[pos+1].y,spherev[pos+1].z);
      glVertex3f(spherev[pos+2].x,spherev[pos+2].y,spherev[pos+2].z);
      polys++;
      glEnd();
      glPopMatrix();
   }   
   glPopMatrix();   
   return polys;
}
      
/* 
 * draw floor pattern
 */   
static int drawpattern(boxedstruct *gp)
{
   int polys = 0;
   if (!gp->gllists[GLL_PATTERN]) {
      glNewList(gp->listobjects + GLL_PATTERN, GL_COMPILE);
     
      glBegin(GL_LINE_STRIP);
      glVertex3f(-25.0f, 0.0f, 35.0f);
      glVertex3f(-15.0f, 0.0f, 35.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-5.0f, 0.0f, 25.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(5.0f, 0.0f, 25.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(15.0f, 0.0f, 35.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(25.0f, 0.0f, 35.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(35.0f, 0.0f, 25.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(35.0f, 0.0f, 15.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(25.0f, 0.0f, 5.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(25.0f, 0.0f, -5.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(35.0f, 0.0f, -15.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(35.0f, 0.0f, -25.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(25.0f, 0.0f, -35.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(15.0f, 0.0f,-35.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(5.0f, 0.0f, -25.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-5.0f, 0.0f, -25.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-15.0f, 0.0f,-35.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-25.0f, 0.0f,-35.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-35.0f, 0.0f, -25.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-35.0f, 0.0f, -15.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-25.0f, 0.0f, -5.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-25.0f, 0.0f, 5.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-35.0f, 0.0f, 15.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-35.0f, 0.0f, 25.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-25.0f, 0.0f, 35.0f); gp->list_polys[GLL_PATTERN]++;
      glEnd();
      
      glBegin(GL_LINE_STRIP);
      glVertex3f(-5.0f, 0.0f, 15.0f);
      glVertex3f(5.0f, 0.0f, 15.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(15.0f, 0.0f, 5.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(15.0f, 0.0f, -5.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(5.0f, 0.0f, -15.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-5.0f, 0.0f, -15.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-15.0f, 0.0f, -5.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-15.0f, 0.0f, 5.0f); gp->list_polys[GLL_PATTERN]++;
      glVertex3f(-5.0f, 0.0f, 15.0f); gp->list_polys[GLL_PATTERN]++;
      glEnd();

      glEndList();
      gp->gllists[GLL_PATTERN] = 1;
   } else {
      glCallList(gp->listobjects + GLL_PATTERN);
      polys += gp->list_polys[GLL_PATTERN];
   }

   return polys;
}
      
      
/*
 * main rendering loop
 */
static void draw(ModeInfo * mi)
{
   boxedstruct *gp = &boxed[MI_SCREEN(mi)];
   int wire = MI_IS_WIREFRAME (mi);
   vectorf v1;
   GLfloat dcam;
   int dx, dz;
   int i;   
   
   GLfloat dgray[4] = {0.3f, 0.3f, 0.3f, 1.0f};
   GLfloat black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
   GLfloat lblue[4] = {0.4f,0.6f,1.0f };
   
   GLfloat l0_ambient[] =    {0.0, 0.0, 0.0, 1.0};
   GLfloat l0_specular[] =    {1.0, 1.0, 1.0, 1.0};
   GLfloat l0_diffuse[] =    {1.0, 1.0, 1.0, 1.0};
   GLfloat l0_position[] =  {0.0, 0.0, 0.0, 1.0}; /* w != 0 -> positional light */
   GLfloat l1_ambient[] =    {0.0, 0.0, 0.0, 1.0};
   GLfloat l1_specular[] =    {1.0, 1.0, 1.0, 1.0};
   GLfloat l1_diffuse[] =    {0.5, 0.5, 0.5, 1.0};
   GLfloat l1_position[] =  {0.0, 1.0, 0.0, 0.0}; /* w = 0 -> directional light */
   
   mi->polygon_count = 0;

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glLoadIdentity();
   
   gp->tic += 0.01f;
   gp->camtic += 0.01f + 0.01f * sin(gp->tic * speed);
   
   /* rotate camera around (0,0,0), looking at (0,0,0), up is (0,1,0) */
   dcam = CAMDISTANCE_MIN + (CAMDISTANCE_MAX - CAMDISTANCE_MIN) + (CAMDISTANCE_MAX - CAMDISTANCE_MIN)*cos((gp->camtic/CAMDISTANCE_SPEED) * speed);
   v1.x = dcam * sin((gp->camtic/gp->cam_x_speed) * speed);
   v1.z = dcam * cos((gp->camtic/gp->cam_z_speed) * speed);
   v1.y = CAM_HEIGHT * sin((gp->camtic/gp->cam_y_speed) * speed) + 1.02 * CAM_HEIGHT;
   gluLookAt(v1.x,v1.y,v1.z,0.0,0.0,0.0,0.0,1.0,0.0); 

   if (!wire) {
     glLightfv(GL_LIGHT0, GL_AMBIENT, l0_ambient); 
     glLightfv(GL_LIGHT0, GL_DIFFUSE, l0_diffuse); 
     glLightfv(GL_LIGHT0, GL_SPECULAR, l0_specular); 
     glLightfv(GL_LIGHT0, GL_POSITION, l0_position);
     glLightfv(GL_LIGHT1, GL_AMBIENT, l1_ambient); 
     glLightfv(GL_LIGHT1, GL_DIFFUSE, l1_diffuse); 
     glLightfv(GL_LIGHT1, GL_SPECULAR, l1_specular); 
     glLightfv(GL_LIGHT1, GL_POSITION, l1_position);
     glEnable(GL_LIGHT0);
     glEnable(GL_LIGHT1);
   
     glFrontFace(GL_CW);
   
     glMaterialfv(GL_FRONT, GL_SPECULAR, black);
     glMaterialfv(GL_FRONT, GL_EMISSION, lblue);
     glMaterialfv(GL_FRONT,GL_AMBIENT,black);
     glMaterialf(GL_FRONT, GL_SHININESS, 5.0);
   }
   
   
   /* draw ground grid */
   /* glDisable(GL_DEPTH_TEST); */
   glDisable(GL_LIGHTING);
   
   glColor3f(0.1,0.1,0.6);
   for (dx= -2; dx<3; dx++) {
      for (dz= -2; dz<3; dz++) {
	 glPushMatrix();
	 glTranslatef(dx*30.0f, 0.0f, dz*30.0f);
	 drawpattern(gp);
	 glPopMatrix();
      }   
   }
   
   /* Set drawing mode for the boxes */
   glEnable(GL_DEPTH_TEST);
   if (!wire) glEnable(GL_TEXTURE_2D);
   glPushMatrix();
   glColor3f(1.0,1.0,1.0);
   glScalef(20.0,0.25,20.0);
   glTranslatef(0.0,2.0,0.0);
   mi->polygon_count += drawfilledbox(gp, wire);
   glPopMatrix();
   glDisable(GL_TEXTURE_2D);

   glPushMatrix();
   glColor3f(0.2,0.5,0.2);
   glScalef(20.0,20.0,0.25);
   glTranslatef(0.0,1.0,81.0);
   mi->polygon_count += drawbox(gp);
   glPopMatrix();

   glPushMatrix();
   glColor3f(0.2,0.5,0.2);
   glScalef(20.0,20.0,0.25);
   glTranslatef(0.0,1.0,-81.0);
   mi->polygon_count += drawbox(gp);
   glPopMatrix();

   glPushMatrix();
   glColor3f(0.2,0.5,0.2);
   glScalef(.25,20.0,20.0);
   glTranslatef(-81.0,1.0,0.0);
   mi->polygon_count += drawbox(gp);
   glPopMatrix();

   glPushMatrix();
   glColor3f(0.2,0.5,0.2);
   glScalef(.25,20.0,20.0);
   glTranslatef(81.0,1.0,0.0);
   mi->polygon_count += drawbox(gp);
   glPopMatrix();

   if (!wire) {
     glEnable(GL_LIGHTING);
   
     glMaterialfv(GL_FRONT, GL_DIFFUSE, dgray);
     glMaterialfv(GL_FRONT, GL_EMISSION, black); /* turn it off before painting the balls */
   }

   /* move the balls and shrapnel */
   updateballs(&gp->bman);

   glFrontFace(GL_CCW);
   for (i=0;i<gp->bman.num_balls;i++) {
      if (gp->bman.balls[i].justcreated) {
	 gp->bman.balls[i].justcreated = FALSE;
	 freetris(&gp->tman[i]);
      }
      if (gp->bman.balls[i].bounced) { 
	 if (gp->tman[i].vertices == NULL) {
	    createtrisfromball(&gp->tman[i],gp->spherev,gp->spherei,SPHERE_INDICES,&gp->bman.balls[i]);
	 } else {
	    updatetris(&gp->tman[i]);
	 }
	 glDisable(GL_CULL_FACE);
	 mi->polygon_count += drawtriman(&gp->tman[i], wire);
	 if (!wire) glEnable(GL_CULL_FACE);
      } else {
         mi->polygon_count += drawball(gp, &gp->bman.balls[i], wire);
      }
   }
      
   glFlush();
}



/* 
 * new window size or exposure 
 */
ENTRYPOINT void reshape_boxed(ModeInfo *mi, int width, int height)
{
   GLfloat     h = (GLfloat) height / (GLfloat) width;
   
   glViewport(0, 0, (GLint) width, (GLint) height);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluPerspective(50.0,1/h,2.0,1000.0);
   glMatrixMode (GL_MODELVIEW);
   
   glLineWidth(1);
   glPointSize(1);   
}


static void
pinit(ModeInfo * mi)
{
   boxedstruct *gp = &boxed[MI_SCREEN(mi)];
   int wire = MI_IS_WIREFRAME (mi);
   ballman *bman;
   int i,texpixels;
   char *texpixeldata;
   char *texpixeltarget;

   glShadeModel(GL_SMOOTH);
   glClearDepth(1.0);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   
   /* Load configuration */
   setdefaultconfig(&gp->config);

   /* give the decay parameter a better curve */
   if (gp->config.decay <= 0.8182) { gp->config.decay = gp->config.decay / 3; }
   else                            { gp->config.decay = (gp->config.decay - 0.75) * 4; }
   
   bman = &gp->bman;
   
   bman->balls = (ball *)malloc(gp->config.numballs * sizeof(ball));
   bman->num_balls = gp->config.numballs;
   bman->ballsize = gp->config.ballsize;
   bman->explosion = gp->config.explosion;
   
   gp->tman = (triman *)malloc(bman->num_balls * sizeof(triman));
   memset(gp->tman,0,bman->num_balls * sizeof(triman));
   
   for(i=0;i<bman->num_balls;i++) {
      gp->tman[i].explosion = (float) (((int)gp->config.explosion) / 15.0f );
      gp->tman[i].decay = gp->config.decay;
      gp->tman[i].momentum = gp->config.momentum;
      gp->tman[i].vertices = NULL;
      gp->tman[i].normals = NULL;
      gp->tman[i].tris = NULL;
      createball(&bman->balls[i]);
      bman->balls[i].loc.y *= rnd();
   }

   generatesphere(gp);
   
   if (!wire) {
     glEnable(GL_CULL_FACE);
     glEnable(GL_LIGHTING);
   }

   /* define cam path */
   gp->cam_x_speed = 1.0f/((float)gp->config.camspeed/50.0 + rnd()*((float)gp->config.camspeed/50.0));
   gp->cam_z_speed = 1.0f/((float)gp->config.camspeed/50.0 + rnd()*((float)gp->config.camspeed/50.0));
   gp->cam_y_speed = 1.0f/((float)gp->config.camspeed/250.0 + rnd()*((float)gp->config.camspeed/250.0));
   if (rnd() < 0.5f) gp->cam_x_speed = -gp->cam_x_speed;
   if (rnd() < 0.5f) gp->cam_z_speed = -gp->cam_z_speed;

   /* define initial cam position */
   gp->tic = gp->camtic = rnd() * 100.0f;
   
   /* define tex1 (bottom plate) */
   gp->tex1 = (char *)malloc(3*width*height*sizeof(GLuint));
   texpixels = 256*256; /*width*height;*/
   texpixeldata = header_data;
   texpixeltarget = gp->tex1;
   for (i=0; i < texpixels; i++) {
	HEADER_PIXEL(texpixeldata,texpixeltarget);
	texpixeltarget += 3;
   }
   
   
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   clear_gl_error();
   i = gluBuild2DMipmaps(GL_TEXTURE_2D, 3, 256, 256,
                         GL_RGB, GL_UNSIGNED_BYTE, gp->tex1);
   if (i)
     {
       const char *s = (char *) gluErrorString (i);
       fprintf (stderr, "%s: error mipmapping texture: %s\n",
                progname, (s ? s : "(unknown)"));
       exit (1);
     }
   check_gl_error("mipmapping");

   glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   
}

 

ENTRYPOINT void
init_boxed(ModeInfo * mi)
{
   int screen = MI_SCREEN(mi);
   
   /* Colormap    cmap; */
   /* Boolean     rgba, doublebuffer, cmap_installed; */
   boxedstruct *gp;

   if (boxed == NULL) {
      if ((boxed = (boxedstruct *) calloc(MI_NUM_SCREENS(mi),sizeof (boxedstruct))) == NULL) return;
   }
   gp = &boxed[screen];
   gp->window = MI_WINDOW(mi);
   
   if ((gp->glx_context = init_GL(mi)) != NULL) {
      reshape_boxed(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
      glDrawBuffer(GL_BACK);
      if (!glIsList(gp->listobjects))	{
	 gp->listobjects = glGenLists(3);
	 gp->gllists[0] = 0;
	 gp->gllists[1] = 0;
	 gp->gllists[2] = 0;
      }
      pinit(mi);
   } else {
      MI_CLEARWINDOW(mi);
   }
}


ENTRYPOINT void
draw_boxed(ModeInfo * mi)
{
   boxedstruct *gp = &boxed[MI_SCREEN(mi)];
   Display    *display = MI_DISPLAY(mi);
   Window      window = MI_WINDOW(mi);
   
   if (!gp->glx_context)
     return;
   
   glDrawBuffer(GL_BACK);
   
   glXMakeCurrent(display, window, *(gp->glx_context));
   draw(mi);
   
   if (mi->fps_p) do_fps (mi);
   glFinish();
   glXSwapBuffers(display, window);
}

ENTRYPOINT void
release_boxed(ModeInfo * mi)
{
   int i;
   
   if (boxed != NULL) {
      int screen;
      
      for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
	 boxedstruct *gp = &boxed[screen];
	 
	 if (gp->glx_context) {
	    /* Display lists MUST be freed while their glXContext is current. */
	    glXMakeCurrent(MI_DISPLAY(mi), gp->window, *(gp->glx_context));
	    
	    if (glIsList(gp->listobjects))
	      glDeleteLists(gp->listobjects, 3);
	    
	    for (i=0;i<gp->bman.num_balls;i++) {
	       if (gp->bman.balls[i].bounced) freetris(&gp->tman[i]);
	    }
	    free (gp->bman.balls);
	    free (gp->tman);
	    free (gp->tex1);
		 
	    
	 }
      }
      (void) free((void *) boxed);
      boxed = NULL;
   }
   FreeAllGL(mi);
}


XSCREENSAVER_MODULE ("Boxed", boxed)

/*********************************************************/

#endif
