#ifndef __THREEJETVEC
#define __THREEJETVEC
#include "i_threejet.h"
#include "i_twojetvec.h"
struct ThreeJetVec {
  ThreeJet x;
  ThreeJet y;
  ThreeJet z;
  operator TwoJetVec() { return TwoJetVec(x,y,z); }
};

ThreeJetVec operator+(ThreeJetVec v, ThreeJetVec w);
ThreeJetVec operator*(ThreeJetVec v, ThreeJet  a);
ThreeJetVec operator*(ThreeJetVec v, double a);
ThreeJetVec AnnihilateVec(ThreeJetVec v, int index);
ThreeJetVec Cross(ThreeJetVec v, ThreeJetVec w);
ThreeJet Dot(ThreeJetVec v, ThreeJetVec w);
TwoJetVec D(ThreeJetVec x, int index);
ThreeJetVec Normalize(ThreeJetVec v);
ThreeJetVec RotateZ(ThreeJetVec v, ThreeJet angle);
ThreeJetVec RotateY(ThreeJetVec v, ThreeJet angle);
ThreeJetVec RotateX(ThreeJetVec v, ThreeJet angle);
ThreeJetVec InterpolateVec(ThreeJetVec v1, ThreeJetVec v2, ThreeJet weight);
ThreeJet Length(ThreeJetVec v);
#endif
