#include <X11/Intrinsic.h>
#include "xlock.h"

#include "i_linkage.h"
#include "i_threejetvec.h"
#include "i_figureeight.h"
#include "i_spline.h"
#include "i_sphere.h"

char *parse_parts(char *parts);
#define	PART_POS 0x1
#define	PART_NEG 0x2

double scale = M_PI;
int scene = 0;
int bezier = 0;
char *parts = NULL;
int n_strips = N_STRIPS;

void
invert_draw(spherestruct * gp)
{
  double umin, vmin, umax, vmax;
  double du, dv;
  double time = 0.0;
  double corrstart, pushstart, twiststart, unpushstart, uncorrstart;
  int binary = 0;
  int j, k;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  glRotatef(gp->view_rotx, 1.0, 0.0, 0.0);
  glRotatef(gp->view_roty, 0.0, 1.0, 0.0);
  glRotatef(gp->view_rotz, 0.0, 0.0, 1.0);
  
  /* default parameters */

  corrstart = 0;
  pushstart = .1;
  twiststart = .23;
  unpushstart = .6;
  uncorrstart = .93;

  vmin = 0.0;
  umin = 0.0;
  vmax = 1.0;
  umax = 1.0;
  du = dv = 1./12.;

  /* Simple inversion fits our needs perfectly... */

  du = 0.04;
  dv = 0.04;
  umax = 2.0;
  parts = "+0+1+2+3+4+5+6+7";
  time = (cos((M_PI*gp->time)/gp->numsteps)+1.0)/2.0;
  binary = 0;
  
  /* draw it here */
  
  if (gp->partlist == NULL)
    gp->partlist = parse_parts(parts);

  /* Need to compute */

  if (gp->construction) {
    glNewList(gp->frames+gp->time, GL_COMPILE);

    if (time >= uncorrstart && uncorrstart >= 0) 
      printScene(UnCorrugate, umin, umax, du, vmin, vmax, dv, 
		 (time - uncorrstart) / (1.0 - uncorrstart), binary);
    else if (time >= unpushstart && unpushstart >= 0) 
      printScene(UnPush, umin, umax, du, vmin, vmax, dv,
		 (time - unpushstart) / 
		 (((uncorrstart < 0) ? 1.0 : uncorrstart) - unpushstart), binary);
    else if (time >= twiststart && twiststart >= 0) 
      printScene(Twist, umin, umax, du, vmin, vmax, dv, 
		 (time - twiststart) / 
		 (((unpushstart < 0) ? 1.0 : unpushstart) - twiststart), binary);
    else if (time >= pushstart && pushstart >= 0) 
      printScene(PushThrough, umin, umax, du, vmin, vmax, dv,
		 (time - pushstart) / 
		 (((twiststart < 0) ? 1.0 : twiststart) - pushstart), binary);
    else if (time >= corrstart && corrstart >= 0) 
      printScene(Corrugate, umin, umax, du, vmin, vmax, dv,
		 (time - corrstart) / 
		 (((pushstart < 0) ? 1.0 : pushstart) - corrstart), binary);
    glEndList();
  }

  for(j = -1; j <= 1; j += 2) {
    for(k = 0; k < n_strips; k++) {
      if(gp->partlist[k] & (j<0 ? PART_NEG : PART_POS)) {
	double t = 2*M_PI * (j < 0 ? n_strips-1-k : k) / n_strips;
	double s = sin(t), c = cos(t);
	
	GLdouble m[16];
	m[0] = j*c; m[4] = -s; m[8] =  0; m[12] = 0;
	m[1] = j*s; m[5] =  c; m[9] =  0, m[13] = 0;
	m[2] =   0; m[6] =  0; m[10] = j; m[14] = 0;
	m[3] =   0; m[7] =  0; m[11] = 0; m[15] = 1;
	
	glPushMatrix();
	glMultMatrixd(m);
	glCallList(gp->frames+gp->time);
	glPopMatrix();
      }
    }
  }

  glPopMatrix();
}

