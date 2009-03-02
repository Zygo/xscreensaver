#ifndef __SPLINE
#define __SPLINE
#include "i_threejetvec.h"
#include "i_twojetvec.h"

typedef TwoJetVec SurfaceTimeFunction(ThreeJet u, ThreeJet v, double t);

void print_point (FILE *fp, TwoJetVec p, double ps, double pus, double pvs, double puvs);
void printSpline(FILE *fp, TwoJetVec v00, TwoJetVec v01, TwoJetVec v10, TwoJetVec v11, double us, double vs, double s0,double s1,double t0,double t1);
void printScene(
  SurfaceTimeFunction *func,
  double umin, double umax, double du,
  double vmin, double vmax, double dv,
  double t, int binary
);
void impossible(char *msg);
#endif
