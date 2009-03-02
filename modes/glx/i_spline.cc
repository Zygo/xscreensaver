#define XLOCK 1
#ifdef XLOCK
#include "xlock.h"
#endif

#include "i_spline.h"
#include "i_evert.h"
extern "C" {
#include <math.h>
}

extern int bezier;
extern char *parts;
extern int n_strips;

void print_point (FILE *fp, TwoJetVec p, double ps, double pus, double pvs, double puvs, int binary)
{
  if(bezier) {
    float xyz[3];
    xyz[0] = double(p.x)*ps + p.x.df_du()*pus/3. + p.x.df_dv()*pvs/3.
             + p.x.d2f_dudv()*puvs/9.;
    xyz[1] = double(p.y)*ps + p.y.df_du()*pus/3. + p.y.df_dv()*pvs/3.
             + p.y.d2f_dudv()*puvs/9.;
    xyz[2] = double(p.z)*ps + p.z.df_du()*pus/3. + p.z.df_dv()*pvs/3.
             + p.z.d2f_dudv()*puvs/9.;
    if (binary) {
      (void) fwrite(&xyz, sizeof(float), 3, fp);
    } else {
      (void) fprintf(fp, "%g %g %g\n", xyz[0], xyz[1], xyz[2]);
    }
  }
  else{
    double x= double(p.x)*ps ;
    double y= double(p.y)*ps ;
    double z= double(p.z)*ps ;
    double nx= p.y.df_du()*p.z.df_dv()-p.z.df_du()*p.y.df_dv();
    double ny= p.z.df_du()*p.x.df_dv()-p.x.df_du()*p.z.df_dv();
    double nz= p.x.df_du()*p.y.df_dv()-p.y.df_du()*p.x.df_dv();
    double s = nx*nx + ny*ny + nz*nz;
    if(s > 0) s = sqrt(1/s);
    (void) fprintf(fp, "%f %f %f    %f %f %f\n", x, y, z, nx*s, ny*s, nz*s);
  }
}

void printMesh(
#ifndef XLOCK
    FILE *fp,
#endif
    TwoJetVec p)
{
    double x= double(p.x) ;
    double y= double(p.y) ;
    double z= double(p.z) ;
    double nx= p.y.df_du()*p.z.df_dv()-p.z.df_du()*p.y.df_dv();
    double ny= p.z.df_du()*p.x.df_dv()-p.x.df_du()*p.z.df_dv();
    double nz= p.x.df_du()*p.y.df_dv()-p.y.df_du()*p.x.df_dv();
    double s = nx*nx + ny*ny + nz*nz;
    if(s > 0) s = sqrt(1/s);
#ifdef XLOCK
    glNormal3d(nx*s, ny*s, nz*s);
    glVertex3d(x, y, z);
#else
    (void) fprintf(fp, "%f %f %f    %f %f %f\n", x, y, z, nx*s, ny*s, nz*s);
#endif
}

void printSpline(FILE *fp, TwoJetVec v00, TwoJetVec v01,
			   TwoJetVec v10, TwoJetVec v11,
			   double us, double vs,
			   double s0, double s1, double t0, double t1, int binary) {
    if (bezier) {
    print_point(fp, v00, 1, 0, 0, 0, binary);
    print_point(fp, v00, 1, us, 0, 0, binary);
    print_point(fp, v10, 1,-us, 0, 0, binary);
    print_point(fp, v10, 1, 0, 0, 0, binary);

    print_point(fp, v00, 1, 0, vs, 0, binary);
    print_point(fp, v00, 1, us, vs, us*vs, binary);
    print_point(fp, v10, 1,-us, vs,-us*vs, binary);
    print_point(fp, v10, 1, 0, vs, 0, binary);

    print_point(fp, v01, 1, 0,-vs, 0, binary);
    print_point(fp, v01, 1, us,-vs,-us*vs, binary);
    print_point(fp, v11, 1,-us,-vs, us*vs, binary);
    print_point(fp, v11, 1, 0,-vs, 0, binary);

    print_point(fp, v01, 1, 0, 0, 0, binary);
    print_point(fp, v01, 1, us, 0, 0, binary);
    print_point(fp, v11, 1,-us, 0, 0, binary);
    print_point(fp, v11, 1, 0, 0, 0, binary);

    if (binary) {
      float sts[8] = {s0,t0, s1,t0, s0,t1, s1,t1};
      (void) fwrite(&sts, sizeof(float), 8, fp);
    } else {
      (void) fprintf(fp, "%g %g  %g %g  %g %g  %g %g\n\n",
  	s0,t0,  s1,t0,  s0,t1, s1,t1);
    }
  }
  else {
    print_point(fp, v00, 1, us, vs, us*vs, binary);
    print_point(fp, v10, 1, us, vs, us*vs, binary);
    print_point(fp, v11, 1, us, vs, us*vs, binary);
    print_point(fp, v01, 1, us, vs, us*vs, binary);
    if(!binary)
	fputc('\n', fp);
  }
}

#define sqr(A) ((A)*(A))

static inline double calcSpeedV(TwoJetVec v) {
  return sqrt(sqr(v.x.df_dv()) + sqr(v.y.df_dv()) + sqr(v.z.df_dv()));
}
static inline double calcSpeedU(TwoJetVec v) {
  return sqrt(sqr(v.x.df_du()) + sqr(v.y.df_du()) + sqr(v.z.df_du()));
}

#define	PART_POS 0x1
#define	PART_NEG 0x2

char *parse_parts(char *parts)
{
    /* Construct matrices to replicate standard unit (u=0..1, v=0..1) into
     * complete sphere.
     */
    char *partlist = (char *)calloc(n_strips, sizeof(char));
    char *cp, *ncp, sign;
    int bits, j;

    for(cp = parts; *cp; ) {
	while((sign = *cp++) == ' ' || sign == ',')
	    ;
	if(sign == '+')
	    bits = PART_POS;
	else if(sign == '-')
	    bits = PART_NEG;
	else {
	    bits = PART_POS|PART_NEG;
	    cp--;
	}
	if(*cp == '*') {
	    for(j = 0; j < n_strips; j++)
		partlist[j] |= bits;
	    cp++;
	} else {
	    j = strtol(cp, &ncp, 0);
	    if(cp == ncp) {
		(void) fprintf(stderr,
"evert -parts: expected string with alternating signs and strip numbers\n");
		return (char *) NULL;
	    }
	    if(j < 0 || j >= n_strips) {
		(void) fprintf(stderr,
"evert -parts: bad strip number %d; must be in range 0..%d\n", j, n_strips-1);
		return (char *) NULL;
	    }
	    partlist[j] |= bits;
	    cp = ncp;
	}
    }
    return partlist;
}

void printScene(
		SurfaceTimeFunction *func,
		double umin, double umax, double adu,
		double vmin, double vmax, double adv,
		double t, int binary
		)

{
  static TwoJetVec **values;
  int j, k;
  int jmax = (int) (fabs(umax-umin)/adu+.5);
  int kmax = (int) (fabs(vmax-vmin)/adv+.5);
  double u, v, du, dv;
  FILE *fp = stdout;

  if(jmax == 0) jmax = 1;
  du = (umax-umin) / jmax;
  if(kmax == 0) kmax = 1;
  dv = (vmax-vmin) / kmax;
  values = (TwoJetVec **) calloc(jmax+1, sizeof(TwoJetVec *));
  double *speedv = (double *) calloc(jmax+1, sizeof(double));
  double **speedu = (double **) calloc(jmax+1, sizeof(double *));
  for (j = 0; j <= jmax; j++) {
    u = umin + j*du;
    values[j] = (TwoJetVec *) calloc(kmax+1, sizeof(TwoJetVec));
    speedu[j] = (double *) calloc(kmax+1, sizeof(double));
    speedv[j] = calcSpeedV((*func)(ThreeJet(u, 1, 0), ThreeJet(0, 0, 1), t));
    if(speedv[j] == 0) {
      /* Perturb a bit, hoping to avoid degeneracy */
      u += (u<1) ? 1e-9 : -1e-9;
      speedv[j] = calcSpeedV((*func)(ThreeJet(u, 1, 0), ThreeJet(0, 0, 1), t));
    }
    for (k = 0; k <= kmax; k++) {
      v = vmin + k*dv;
      values[j][k] = (*func)(
       ThreeJet(u, 1, 0),
       ThreeJet(v, 0, 1),
       t
      );
      speedu[j][k] = calcSpeedU(values[j][k]);
    }
  }
#if 0
  (void) fprintf(fp, "Declare \"speeds\" \"varying float\"\n");
  (void) fprintf(fp, "Declare \"speedt\" \"varying float\"\n");
#endif
#ifndef XLOCK
  if(parts != NULL) {
    /* Construct matrices to replicate standard unit (u=0..1, v=0..1) into
     * complete sphere.
     */
    char *partlist = parse_parts(parts);

    if(partlist == NULL)
	return;

    (void) fprintf(fp, "{ INST transforms { TLIST\n");
    for(j = -1; j <= 1; j += 2) {
	for(k = 0; k < n_strips; k++) {
	  if(partlist[k] & (j<0 ? PART_NEG : PART_POS)) {
	    double t = 2*M_PI * (j < 0 ? n_strips-1-k : k) / n_strips;
	    double s = sin(t), c = cos(t);

	    (void) fprintf(fp, "# %c%d of %d\n", j<0 ? '-' : '+', k, n_strips);
	    (void) fprintf(fp, "\t%10f %10f %10f %10f\n", j*c, -s,	     0., 0.);
	    (void) fprintf(fp, "\t%10f %10f %10f %10f\n", j*s,  c,	     0., 0.);
	    (void) fprintf(fp, "\t%10f %10f %10f %10f\n", 0.,   0., (double)j, 0.);
	    (void) fprintf(fp, "\t%10f %10f %10f %10f\n", 0.,   0.,	     0., 1.);
	  }
	}
    }
    (void) fprintf(fp, "}\ngeom ");
  }
#endif

  if(bezier) {
    (void) fprintf(fp, "{ STBBP%s\n", binary ? " BINARY" : "");
    for (j = 0; j < jmax; j++) {
      u = umin + j*du;
      for (k = 0; k < kmax; k++) {
	v = vmin + k*dv;
	printSpline(fp, values[j][k], values[j][k+1],
		  values[j+1][k], values[j+1][k+1],
		  du, dv,
		  umin+j*du, umin+(j+1)*du,  vmin+k*dv, vmin+(k+1)*dv, binary);
      }
    }
  }
  else {
#ifndef XLOCK
    int nu = kmax+1, nv = jmax+1;
    (void) fprintf(fp, "{ NMESH%s\n", binary ? " BINARY" : "");

    if(binary) {
	(void) fwrite(&nu, sizeof(int), 1, stdout);
	(void) fwrite(&nv, sizeof(int), 1, stdout);
    } else {
	(void) fprintf(fp, "%d %d\n", nu, nv);
    }
#endif

#ifdef XLOCK
    for(j = 1; j <= jmax; j++) {
        glBegin(GL_QUAD_STRIP);
        for(k = 0; k <= kmax; k++) {
	    printMesh(values[j-1][k]);
	    printMesh(values[j][k]);
	}
	glEnd();
#else
    for(j = 0; j <= jmax; j++) {
        for(k = 0; k <= kmax; k++)
	    printMesh(fp, values[j][k]);

	if(!binary)
	    fputc('\n', fp);
#endif

    }
  }

#ifndef XLOCK
  if(parts)
    (void) fprintf(fp, " }\n");
  (void) fprintf(fp, "}\n");
#endif

  for (j = 0; j <= jmax; j++) {
    if (values[j] != NULL) {
      (void) free((void *) values[j]);
      values[j] = (TwoJetVec *) NULL;
    }
    if (speedu[j] != NULL) {
      (void) free((void *) speedu[j]);
      speedu[j] = (double *) NULL;
    }
  }
  (void) free((void *) values);
  (void) free((void *) speedu);
  (void) free((void *) speedv);
}

void impossible(char *msg) {
  (void) fprintf(stderr, "%s\n", msg);
  exit(1);
}

