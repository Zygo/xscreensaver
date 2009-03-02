#ifndef __TWOJETVEC
#define __TWOJETVEC
#include "i_twojet.h"
struct TwoJetVec {
  TwoJet x;
  TwoJet y;
  TwoJet z;
  TwoJetVec() {}
  TwoJetVec(TwoJet a, TwoJet b, TwoJet c) { x = a; y = b; z = c; }
};

TwoJetVec operator+(TwoJetVec v, TwoJetVec w);
TwoJetVec operator*(TwoJetVec v, TwoJet  a);
TwoJetVec operator*(TwoJetVec v, double a);
TwoJetVec AnnihilateVec(TwoJetVec v, int index);
TwoJetVec Cross(TwoJetVec v, TwoJetVec w);
TwoJet Dot(TwoJetVec v, TwoJetVec w);
TwoJetVec Normalize(TwoJetVec v);
TwoJetVec RotateZ(TwoJetVec v, TwoJet angle);
TwoJetVec RotateY(TwoJetVec v, TwoJet angle);
TwoJetVec RotateX(TwoJetVec v, TwoJet angle);
TwoJetVec InterpolateVec(TwoJetVec v1, TwoJetVec v2, TwoJet weight);
TwoJet Length(TwoJetVec v);
#endif
