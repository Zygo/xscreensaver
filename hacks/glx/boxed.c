/* thebox --- 3D bouncing balls that explode */

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
 */

#include <X11/Intrinsic.h>
#include "boxed.h"

/*
**----------------------------------------------------------------------------
** Defines
**----------------------------------------------------------------------------
*/

#ifdef STANDALONE
# define PROGCLASS					"boxed"
# define HACK_INIT					init_boxed
# define HACK_DRAW					draw_boxed
# define HACK_RESHAPE				        reshape_boxed
# define boxed_opts					xlockmore_opts
# define DEFAULTS	"*delay:		20000   \n"			\
     "*showFPS:         False   \n"			\

# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL
#include <GL/glu.h>

#undef countof 
#define countof(x) (int)(sizeof((x))/sizeof((*x)))
#undef rnd
#define rnd() (frand(1.0))

/* #define DEF_PLANETARY "False"

static int planetary;

static XrmOptionDescRec opts[] = {
  {"-planetary", ".gears.planetary", XrmoptionNoArg, (caddr_t) "true" },
  {"+planetary", ".gears.planetary", XrmoptionNoArg, (caddr_t) "false" },
};

static argtype vars[] = {
  {(caddr_t *) &planetary, "planetary", "Planetary", DEF_PLANETARY, t_Bool},
};
*/

/* ModeSpecOpts boxed_opts = {countof(opts), opts, countof(vars), vars, NULL}; */

ModeSpecOpt boxed_opts = {0, NULL, 0, NULL, NULL};

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

/* rendering defines */

/* box size */
#define BOX_SIZE 	20.0f

/* camera */
#define CAM_HEIGHT 	100.0f
#define CAMDISTANCE_MIN 20.0
#define CAMDISTANCE_MAX 150.0
#define CAMDISTANCE_SPEED 1.5

/* rendering the sphere */
#define MESH_SIZE	5
#define SPHERE_VERTICES	(2+MESH_SIZE*MESH_SIZE*2)
#define SPHERE_INDICES	((MESH_SIZE*4 + MESH_SIZE*4*(MESH_SIZE-1))*3)

#define EXPLOSION 10.0f
#define MAXBALLS  50;
#define NUMBALLS 12;
#define BALLSIZE 3.0f;

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
} tri;

typedef struct {
   int		num_tri;
   int 		lifetime;
   float	scalefac;
   float	explosion;
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
   BOOL textures;
   BOOL transparent;
   float camspeed;
} boxed_config;


typedef struct {
   float          cam_x_speed, cam_z_speed, cam_y_speed;
   boxed_config  config;
   float	  tic;
   vectorf        spherev[SPHERE_VERTICES];
   GLint          spherei[SPHERE_INDICES];
   ballman        bman;
   triman         *tman;
   GLXContext     *glx_context;
   GLuint         listobjects;
   GLuint         gllists[3];
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



/*
 * Generate the Sphere data
 * 
 * Input: 
 */ 

static void generatesphere(void)
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
   
   spherei = boxed->spherei;
   spherev = boxed->spherev;
   
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

void createball(ball *newball) {
   float r=0.0f,g=0.0f,b=0.0f;
   newball->loc.x = 5-10*rnd();
   newball->loc.y = 35+20*rnd();
   newball->loc.z = 5-10*rnd();
   newball->dir.x = 0.5f-rnd();
   newball->dir.y = 0.0;
   newball->dir.z = 0.5-rnd();
   newball->offside = 0;
   newball->bounced = FALSE;
   newball->radius = BALLSIZE;
   while (r+g+b < 1.7f ) {
      newball->color.x = r=rnd();
      newball->color.y = g=rnd();
      newball->color.z = b=rnd();
   }
   newball->justcreated = TRUE;
}

/* Update position of each ball */

void updateballs(ballman *bman) {
   register int b,j;
   vectorf dvect,richting,relspeed,influence;
   GLfloat squaredist;

   for (b=0;b<bman->num_balls;b++) {

      /* apply gravity */
      bman->balls[b].dir.y -= 0.15f;
      /* apply movement */
      addvectors(&bman->balls[b].loc,&bman->balls[b].loc,&bman->balls[b].dir);
      /* boundary check */
      if (bman->balls[b].loc.y < bman->balls[b].radius) { /* ball onder bodem? (bodem @ y=0) */
	 if ((bman->balls[b].loc.x < -100.0) || 
	     (bman->balls[b].loc.x > 100.0) ||
	     (bman->balls[b].loc.z < -100.0) ||
	     (bman->balls[b].loc.z > 100.0)) {
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

void createtrisfromball(triman* tman, vectorf *spherev, GLint *spherei, int ind_num, ball *b) {
   int pos;
   float explosion;
   float scale;
   register int i;
   vectorf avgdir,dvect;

   tman->scalefac = b->radius;
   copyvector(&tman->color,&b->color);
   explosion = 1.0f + tman->explosion * 2.0 * rnd();

   tman->num_tri = ind_num/3;
   
   /* reserveer geheugen voor de poly's in een bal */
   
   tman->tris = (tri *)malloc(tman->num_tri * sizeof(tri));
   tman->vertices = (vectorf *)malloc(ind_num * sizeof(vectorf));
   tman->normals = (vectorf *)malloc(ind_num/3 * sizeof(vectorf));
   
   for (i=0; i<(tman->num_tri); i++) {
      tman->tris[i].far = FALSE;
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
            
      /* bereken nieuwe richting */
      scalevector(&tman->tris[i].dir,&avgdir,explosion);
      dvect.x = 0.1f - 0.2f*rnd();
      dvect.y = 0.15f - 0.3f*rnd();
      dvect.z = 0.1f - 0.2f*rnd(); 
      addvectors(&tman->tris[i].dir,&tman->tris[i].dir,&dvect);
   }
}


/*
* update position of each tri
*/

void updatetris(triman *t) {
   int b;
   GLfloat xd,zd;
   
   for (b=0;b<t->num_tri;b++) {
      /* apply gravity */
      t->tris[b].dir.y -= 0.1f;
      /* apply movement */
      addvectors(&t->tris[b].loc,&t->tris[b].loc,&t->tris[b].dir);
      /* boundary check */
      if (t->tris[b].far) continue;
      if (t->tris[b].loc.y < 0) { /* onder bodem ? */
	 if ((t->tris[b].loc.x > -100.0f) &
	     (t->tris[b].loc.x < 100.0f) &
	     (t->tris[b].loc.z > -100.0f) &
	     (t->tris[b].loc.z < 100.0f)) {  /* in veld  */
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
void freetris(triman *t) {
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
void setdefaultconfig(boxed_config *config) {
  config->numballs = NUMBALLS;
  config->textures = TRUE;
  config->transparent = FALSE;
  config->explosion = 25.0f;
  config->ballsize = BALLSIZE;
  config->camspeed = 35.0f;
}


/*
 * draw bottom
 */ 
static void drawfilledbox(boxedstruct *boxed)
{   
   /* draws texture filled box, 
      top is drawn using the entire texture, 
      the sides are drawn using the edge of the texture
    */
   
   /* front */
   glBegin(GL_QUADS);
   glTexCoord2f(0,1);
   glVertex3f(-1.0,1.0,1.0);
   glTexCoord2f(1,1);
   glVertex3f(1.0,1.0,1.0);
   glTexCoord2f(1,1);
   glVertex3f(1.0,-1.0,1.0);
   glTexCoord2f(0,1);
   glVertex3f(-1.0,-1.0,1.0);
   /* rear */
   glTexCoord2f(0,1);
   glVertex3f(1.0,1.0,-1.0);
   glTexCoord2f(1,1);
   glVertex3f(-1.0,1.0,-1.0);
   glTexCoord2f(1,1);
   glVertex3f(-1.0,-1.0,-1.0);
   glTexCoord2f(0,1);
   glVertex3f(1.0,-1.0,-1.0);
   /* left */
   glTexCoord2f(1,1);
   glVertex3f(-1.0,1.0,1.0);
   glTexCoord2f(1,1);
   glVertex3f(-1.0,-1.0,1.0);
   glTexCoord2f(0,1);
   glVertex3f(-1.0,-1.0,-1.0);
   glTexCoord2f(0,1);
   glVertex3f(-1.0,1.0,-1.0);
   /* right */
   glTexCoord2f(0,1);
   glVertex3f(1.0,1.0,1.0);
   glTexCoord2f(1,1);
   glVertex3f(1.0,1.0,-1.0);
   glTexCoord2f(1,1);
   glVertex3f(1.0,-1.0,-1.0);
   glTexCoord2f(0,1);
   glVertex3f(1.0,-1.0,1.0);
   /* top */
   glTexCoord2f(0.0,0.0);
   glVertex3f(-1.0,1.0,1.0);
   glTexCoord2f(0.0,1.0);
   glVertex3f(-1.0,1.0,-1.0);
   glTexCoord2f(1.0,1.0);
   glVertex3f(1.0,1.0,-1.0);
   glTexCoord2f(1.0,0.0);
   glVertex3f(1.0,1.0,1.0);
   /* bottom */
   glTexCoord2f(0,0);
   glVertex3f(-1.0,-1.0,1.0);
   glTexCoord2f(0,1);
   glVertex3f(-1.0,-1.0,-1.0);
   glTexCoord2f(1,1);
   glVertex3f(1.0,-1.0,-1.0);
   glTexCoord2f(1,0);
   glVertex3f(1.0,-1.0,1.0);
   glEnd();
}


/*
 * Draw a box made of lines
 */ 
static void drawbox(boxedstruct *boxed)
{
   /* top */
   glBegin(GL_LINE_STRIP);
   glVertex3f(-1.0,1.0,1.0);
   glVertex3f(-1.0,1.0,-1.0);
   glVertex3f(1.0,1.0,-1.0);
   glVertex3f(1.0,1.0,1.0);
   glVertex3f(-1.0,1.0,1.0);
   glEnd();
   /* bottom */
   glBegin(GL_LINE_STRIP);
   glVertex3f(-1.0,-1.0,1.0);
   glVertex3f(1.0,-1.0,1.0);
   glVertex3f(1.0,-1.0,-1.0);
   glVertex3f(-1.0,-1.0,-1.0);
   glVertex3f(-1.0,-1.0,1.0);
   glEnd();
   /* connect top & bottom */
   glBegin(GL_LINES);
   glVertex3f(-1.0,1.0,1.0);
   glVertex3f(-1.0,-1.0,1.0);
   glVertex3f(1.0,1.0,1.0);
   glVertex3f(1.0,-1.0,1.0);
   glVertex3f(1.0,1.0,-1.0);
   glVertex3f(1.0,-1.0,-1.0);
   glVertex3f(-1.0,1.0,-1.0);
   glVertex3f(-1.0,-1.0,-1.0);
   glEnd();
}


  
/* 
 * Draw ball
 */
static void drawball(boxedstruct *gp, ball *b)
{
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
	 glBegin(GL_TRIANGLES);
	 glNormal3f(spherev[spherei[pos+0]].x,spherev[spherei[pos+0]].y,spherev[spherei[pos+0]].z);
	 glVertex3f(spherev[spherei[pos+0]].x,spherev[spherei[pos+0]].y,spherev[spherei[pos+0]].z);
	 glNormal3f(spherev[spherei[pos+1]].x,spherev[spherei[pos+1]].y,spherev[spherei[pos+1]].z);
	 glVertex3f(spherev[spherei[pos+1]].x,spherev[spherei[pos+1]].y,spherev[spherei[pos+1]].z);
	 glNormal3f(spherev[spherei[pos+2]].x,spherev[spherei[pos+2]].y,spherev[spherei[pos+2]].z);
	 glVertex3f(spherev[spherei[pos+2]].x,spherev[spherei[pos+2]].y,spherev[spherei[pos+2]].z);
	 glEnd();
      }
      glEndList();
      gp->gllists[GLL_BALL] = 1;
   } else {
      glCallList(gp->listobjects + GLL_BALL);
   }
   
   glPopMatrix();
}

    
/* 
 * Draw all triangles in triman
 */
static void drawtriman(triman *t) 
{
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
      pos = i*3;
      glPushMatrix();
      glTranslatef(t->tris[i].loc.x,t->tris[i].loc.y,t->tris[i].loc.z);
      glBegin(GL_TRIANGLES);
      glNormal3f(t->normals[i].x,t->normals[i].y,t->normals[i].z);
      glVertex3f(spherev[pos+0].x,spherev[pos+0].y,spherev[pos+0].z);
      glVertex3f(spherev[pos+1].x,spherev[pos+1].y,spherev[pos+1].z);
      glVertex3f(spherev[pos+2].x,spherev[pos+2].y,spherev[pos+2].z);
      glEnd();
      glPopMatrix();
   }   
   glPopMatrix();   
}
      
/* 
 * draw floor pattern
 */   
static void drawpattern(boxedstruct *boxed)
{
   if (!boxed->gllists[GLL_PATTERN]) {
      glNewList(boxed->listobjects + GLL_PATTERN, GL_COMPILE);
     
      glBegin(GL_LINE_STRIP);
      glVertex3f(-25.0f, 0.0f, 35.0f);
      glVertex3f(-15.0f, 0.0f, 35.0f);
      glVertex3f(-5.0f, 0.0f, 25.0f);
      glVertex3f(5.0f, 0.0f, 25.0f);
      glVertex3f(15.0f, 0.0f, 35.0f);
      glVertex3f(25.0f, 0.0f, 35.0f);
      glVertex3f(35.0f, 0.0f, 25.0f);
      glVertex3f(35.0f, 0.0f, 15.0f);
      glVertex3f(25.0f, 0.0f, 5.0f);
      glVertex3f(25.0f, 0.0f, -5.0f);
      glVertex3f(35.0f, 0.0f, -15.0f);
      glVertex3f(35.0f, 0.0f, -25.0f);
      glVertex3f(25.0f, 0.0f, -35.0f);
      glVertex3f(15.0f, 0.0f,-35.0f);
      glVertex3f(5.0f, 0.0f, -25.0f);
      glVertex3f(-5.0f, 0.0f, -25.0f);
      glVertex3f(-15.0f, 0.0f,-35.0f);
      glVertex3f(-25.0f, 0.0f,-35.0f);
      glVertex3f(-35.0f, 0.0f, -25.0f);
      glVertex3f(-35.0f, 0.0f, -15.0f);
      glVertex3f(-25.0f, 0.0f, -5.0f);
      glVertex3f(-25.0f, 0.0f, 5.0f);
      glVertex3f(-35.0f, 0.0f, 15.0f);
      glVertex3f(-35.0f, 0.0f, 25.0f);
      glVertex3f(-25.0f, 0.0f, 35.0f);
      glEnd();
      
      glBegin(GL_LINE_STRIP);
      glVertex3f(-5.0f, 0.0f, 15.0f);
      glVertex3f(5.0f, 0.0f, 15.0f);
      glVertex3f(15.0f, 0.0f, 5.0f);
      glVertex3f(15.0f, 0.0f, -5.0f);
      glVertex3f(5.0f, 0.0f, -15.0f);
      glVertex3f(-5.0f, 0.0f, -15.0f);
      glVertex3f(-15.0f, 0.0f, -5.0f);
      glVertex3f(-15.0f, 0.0f, 5.0f);
      glVertex3f(-5.0f, 0.0f, 15.0f);
      glEnd();

      glEndList();
      boxed->gllists[GLL_PATTERN] = 1;
   } else {
      glCallList(boxed->listobjects + GLL_PATTERN);
   }	

}
      
      
/*
 * main rendering loop
 */
static void draw(ModeInfo * mi)
{
   boxedstruct *gp = &boxed[MI_SCREEN(mi)];
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
   
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glLoadIdentity();
   
   gp->tic += 0.01f;

   /* rotate camera around (0,0,0), looking at (0,0,0), up is (0,1,0) */
   dcam = CAMDISTANCE_MIN + (CAMDISTANCE_MAX - CAMDISTANCE_MIN) + (CAMDISTANCE_MAX - CAMDISTANCE_MIN)*cos(gp->tic/CAMDISTANCE_SPEED);
   v1.x = dcam * sin(gp->tic/gp->cam_x_speed);
   v1.z = dcam * cos(gp->tic/gp->cam_z_speed);
   v1.y = CAM_HEIGHT * sin(gp->tic/gp->cam_y_speed) + 1.02 * CAM_HEIGHT;
   gluLookAt(v1.x,v1.y,v1.z,0.0,0.0,0.0,0.0,1.0,0.0); 

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
   glEnable(GL_TEXTURE_2D);
   glPushMatrix();
   glColor3f(1.0,1.0,1.0);
   glScalef(20.0,0.25,20.0);
   glTranslatef(0.0,2.0,0.0);
   drawfilledbox(gp);
   glPopMatrix();
   glDisable(GL_TEXTURE_2D);

   glPushMatrix();
   glColor3f(0.2,0.5,0.2);
   glScalef(20.0,20.0,0.25);
   glTranslatef(0.0,1.0,81.0);
   drawbox(gp);
   glPopMatrix();

   glPushMatrix();
   glColor3f(0.2,0.5,0.2);
   glScalef(20.0,20.0,0.25);
   glTranslatef(0.0,1.0,-81.0);
   drawbox(gp);
   glPopMatrix();

   glPushMatrix();
   glColor3f(0.2,0.5,0.2);
   glScalef(.25,20.0,20.0);
   glTranslatef(-81.0,1.0,0.0);
   drawbox(gp);
   glPopMatrix();

   glPushMatrix();
   glColor3f(0.2,0.5,0.2);
   glScalef(.25,20.0,20.0);
   glTranslatef(81.0,1.0,0.0);
   drawbox(gp);
   glPopMatrix();

   glEnable(GL_LIGHTING);
   
   glMaterialfv(GL_FRONT, GL_DIFFUSE, dgray);
   glMaterialfv(GL_FRONT, GL_EMISSION, black); /* turn it off before painting the balls */

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
	 drawtriman(&gp->tman[i]);
	 glEnable(GL_CULL_FACE);
      } else {
	 drawball(gp, &gp->bman.balls[i]);
      }
   }
      
   glFlush();
}



/* 
 * new window size or exposure 
 */
void reshape_boxed(ModeInfo *mi, int width, int height)
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
   ballman *bman;
   int i,texpixels;
   char *texpixeldata;
   char *texpixeltarget;

   glShadeModel(GL_SMOOTH);
   glClearDepth(1.0);
   glClearColor(0.0,0.05,0.1,0.0);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   
   /* Load configuration */
   setdefaultconfig(&gp->config);
   
   bman = &gp->bman;
   
   bman->balls = (ball *)malloc(gp->config.numballs * sizeof(ball));
   bman->num_balls = gp->config.numballs;
   bman->ballsize = gp->config.ballsize;
   bman->explosion = gp->config.explosion;
   
   gp->tman = (triman *)malloc(bman->num_balls * sizeof(triman));
   memset(gp->tman,0,bman->num_balls * sizeof(triman));
   
   for(i=0;i<bman->num_balls;i++) {
      gp->tman[i].explosion = (float) (((int)gp->config.explosion) / 15.0f );
      gp->tman[i].vertices = NULL;
      gp->tman[i].normals = NULL;
      gp->tman[i].tris = NULL;
      createball(&bman->balls[i]);
      bman->balls[i].loc.y *= rnd();
   }

   generatesphere();
   
   glEnable(GL_CULL_FACE);
   glEnable(GL_LIGHTING);

   /* define cam path */
   gp->cam_x_speed = 1.0f/((float)gp->config.camspeed/50.0 + rnd()*((float)gp->config.camspeed/50.0));
   gp->cam_z_speed = 1.0f/((float)gp->config.camspeed/50.0 + rnd()*((float)gp->config.camspeed/50.0));
   gp->cam_y_speed = 1.0f/((float)gp->config.camspeed/250.0 + rnd()*((float)gp->config.camspeed/250.0));
   if (rnd() < 0.5f) gp->cam_x_speed = -gp->cam_x_speed;
   if (rnd() < 0.5f) gp->cam_z_speed = -gp->cam_z_speed;
   
   
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

 

void
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


void
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

void
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


/*********************************************************/

#endif
