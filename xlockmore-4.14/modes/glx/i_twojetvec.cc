#include "i_twojetvec.h"
TwoJetVec operator+(TwoJetVec v, TwoJetVec w) {
  TwoJetVec result;
  result.x = v.x + w.x;
  result.y = v.y + w.y;
  result.z = v.z + w.z;
  return result;
}

TwoJetVec operator*(TwoJetVec v, TwoJet  a) {
  TwoJetVec result;
  result.x = v.x*a;
  result.y = v.y*a;
  result.z = v.z*a;
  return result;
}

TwoJetVec operator*(TwoJetVec v, double a) {
  TwoJetVec result;
  result.x = v.x*a;
  result.y = v.y*a;
  result.z = v.z*a;
  return result;
}

TwoJetVec AnnihilateVec(TwoJetVec v, int index) {
  TwoJetVec result;
  result.x = Annihilate(v.x, index);
  result.y = Annihilate(v.y, index);
  result.z = Annihilate(v.z, index);
  return result;
}

TwoJetVec Cross(TwoJetVec v, TwoJetVec w) {
  TwoJetVec result;
  result.x = v.y*w.z + v.z*w.y*-1;
  result.y = v.z*w.x + v.x*w.z*-1;
  result.z = v.x*w.y + v.y*w.x*-1;
  return result;
}

TwoJet Dot(TwoJetVec v, TwoJetVec w) {
  return v.x*w.x + v.y*w.y + v.z*w.z;
}

TwoJetVec Normalize(TwoJetVec v) {
  TwoJet a;
  a = Dot(v,v);
  if (a > 0)
    a = a^-0.5;
  else
    a = TwoJet(0, 0, 0);
  return v*a;
}

TwoJetVec RotateZ(TwoJetVec v, TwoJet angle) {
  TwoJetVec result;
  TwoJet s,c;
  s = Sin (angle);
  c = Cos (angle);
  result.x =          v.x*c + v.y*s;
  result.y = v.x*s*-1 + v.y*c;
  result.z = v.z;
  return result;
}

TwoJetVec RotateY(TwoJetVec v, TwoJet angle) {
  TwoJetVec result;
  TwoJet s, c;
  s = Sin (angle);
  c = Cos (angle);
  result.x = v.x*c + v.z*s*-1;
  result.y = v.y;
  result.z = v.x*s + v.z*c    ;
  return result;
}

TwoJetVec RotateX(TwoJetVec v, TwoJet angle) {
  TwoJetVec result;
  TwoJet s,c;
  s = Sin (angle);
  c = Cos (angle);
  result.x = v.x;
  result.y = v.y*c + v.z*s;
  result.z = v.y*s*-1 + v.z*c;
  return result;
}

TwoJetVec InterpolateVec(TwoJetVec v1, TwoJetVec v2, TwoJet weight) {
  return (v1) * (weight*-1 + 1) + v2*weight;
}

TwoJet Length(TwoJetVec v)
{
  return (v.x^2 + v.y^2) ^ (.5);
}
