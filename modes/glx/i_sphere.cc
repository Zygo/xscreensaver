#include "i_threejetvec.h"
#include "i_figureeight.h"
#include "i_spline.h"
#include "i_sphere.h"

ThreeJetVec Arc(ThreeJet u, ThreeJet v, double xsize, double ysize, double zsize) {
  ThreeJetVec result;
  u = u*0.25;
  result.x = Sin (u) * Sin (v) * xsize;
  result.y = Sin (u) * Cos (v) * ysize;
  result.z = Cos (u) * zsize;
  return result;
}

ThreeJetVec Straight(ThreeJet u, ThreeJet v, double xsize, double ysize, double zsize) {
  ThreeJetVec result;
  u = u*0.25;
#if 0
  u = (u) * (-0.15915494) + 1; /* 1/2pi */
#endif
  result.x = Sin (v) * xsize;
  result.y = Cos (v) * ysize;
  result.z = Cos (u) * zsize;
  return result;
}

ThreeJet Param1(ThreeJet x) {
  double offset = 0;
  x %= 4;
  if (x > 2) { x = x+(-2); offset = 2; }
  if (x <= 1) return x*2 + (x^2)*(-1) + offset;
  else return (x^2) + x*(-2) + (2 + offset);
}

ThreeJet Param2(ThreeJet x) {
  double offset = 0;
  x %= 4;
  if (x > 2) { x = x+(-2); offset = 2; }
  if (x <= 1) return (x^2) + offset;
  else return (x^2)*(-1) + x*4 + (-2 + offset);
}

static inline ThreeJet TInterp(double x) {
  return ThreeJet(x,0,0);
}

ThreeJet UInterp(ThreeJet x) {
  x %= 2;
  if (x > 1)
    x = x*(-1) + 2;
  return (x^2)*3 + (x^3) * (-2);
}

#define FFPOW 3
ThreeJet FFInterp(ThreeJet x) {
  x %= 2;
  if (x > 1)
    x = x*(-1) + 2;
  x = x*1.06 + -0.05;
  if (x < 0) return ThreeJet(0, 0, 0);
  else if (x > 1) return ThreeJet(0, 0, 0) + 1;
  else return (x ^ (FFPOW-1)) * (FFPOW) + (x^FFPOW) * (-FFPOW+1);
}

#define FSPOW 3
ThreeJet FSInterp(ThreeJet x) {
  x %= 2;
  if (x > 1)
    x = x*(-1) + 2;
  return ((x ^ (FSPOW-1)) * (FSPOW) + (x^FSPOW) * (-FSPOW+1)) * (-0.2);
}

ThreeJetVec Stage0(ThreeJet u, ThreeJet v) {
  return Straight(u, v, 1, 1, 1);
}

ThreeJetVec Stage1(ThreeJet u, ThreeJet v) {
  return Arc(u, v, 1, 1, 1);
}

ThreeJetVec Stage2(ThreeJet u, ThreeJet v) {
  return InterpolateVec(
    Arc(Param1(u), v, 0.9, 0.9, -1),
    Arc(Param2(u), v, 1, 1, 0.5),
    UInterp(u));
}

ThreeJetVec Stage3(ThreeJet u, ThreeJet v) {
  return InterpolateVec(
    Arc(Param1(u), v,-0.9,-0.9,-1),
    Arc(Param2(u), v,-1, 1,-0.5),
    UInterp(u));
}

ThreeJetVec Stage4(ThreeJet u, ThreeJet v) {
  return Arc(u, v, -1,-1, -1);
}

ThreeJetVec Scene01(ThreeJet u, ThreeJet v, double t) {
  return InterpolateVec(Stage0(u,v), Stage1(u,v), TInterp(t));
}

ThreeJetVec Scene12(ThreeJet u, ThreeJet v, double t) {
  return InterpolateVec(Stage1(u,v), Stage2(u,v), TInterp(t));
}

ThreeJetVec Scene23(ThreeJet u, ThreeJet v, double t) {
  t = TInterp(t) * 0.5;
  double tt = (u <= 1) ? t : -t;
  return InterpolateVec(
    RotateZ(Arc(Param1(u), v, 0.9, 0.9,-1), ThreeJet(tt,0,0)),
    RotateY(Arc(Param2(u), v, 1, 1, 0.5), ThreeJet(t,0,0)),
    UInterp(u)
  );
}

ThreeJetVec Scene34(ThreeJet u, ThreeJet v, double t) {
  return InterpolateVec(Stage3(u,v), Stage4(u,v), TInterp(t));
}

TwoJetVec BendIn(ThreeJet u, ThreeJet v, double t) {
  t = TInterp(t);
  return AddFigureEight(
    Scene01(u, ThreeJet(0, 0, 1), t),
    u, v, ThreeJet(0, 0, 0), FSInterp(u));
}
TwoJetVec Corrugate(ThreeJet u, ThreeJet v, double t) {
  t = TInterp(t);
  return AddFigureEight(
    Stage1(u, ThreeJet(0, 0, 1)),
    u, v, FFInterp(u) * ThreeJet(t,0,0), FSInterp(u));
}
TwoJetVec PushThrough(ThreeJet u, ThreeJet v, double t) {
  return AddFigureEight(
    Scene12(u,ThreeJet(0, 0, 1),t),
    u, v, FFInterp(u), FSInterp(u));
}
TwoJetVec Twist(ThreeJet u, ThreeJet v, double t) {
  return AddFigureEight(
    Scene23(u,ThreeJet(0, 0, 1),t),
    u, v, FFInterp(u), FSInterp(u));
}

TwoJetVec UnPush(ThreeJet u, ThreeJet v, double t) {
  return AddFigureEight(
    Scene34(u,ThreeJet(0, 0, 1),t),
    u, v, FFInterp(u), FSInterp(u));
}

TwoJetVec UnCorrugate(ThreeJet u, ThreeJet v, double t) {
  t = TInterp((t) * (-1) + 1);
  return AddFigureEight(
    Stage4(u,ThreeJet(0, 0, 1)),
    u, v, FFInterp(u) * ThreeJet(t,0,0), FSInterp(u));
}
