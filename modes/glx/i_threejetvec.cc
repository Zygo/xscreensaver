#include "i_threejetvec.h"
ThreeJetVec operator+(ThreeJetVec v, ThreeJetVec w) {
  ThreeJetVec result;
  result.x = v.x + w.x;
  result.y = v.y + w.y;
  result.z = v.z + w.z;
  return result;
}

ThreeJetVec operator*(ThreeJetVec v, ThreeJet  a) {
  ThreeJetVec result;
  result.x = v.x*a;
  result.y = v.y*a;
  result.z = v.z*a;
  return result;
}

ThreeJetVec operator*(ThreeJetVec v, double a) {
  ThreeJetVec result;
  result.x = v.x*a;
  result.y = v.y*a;
  result.z = v.z*a;
  return result;
}

ThreeJetVec AnnihilateVec(ThreeJetVec v, int index) {
  ThreeJetVec result;
  result.x = Annihilate(v.x, index);
  result.y = Annihilate(v.y, index);
  result.z = Annihilate(v.z, index);
  return result;
}

TwoJetVec D(ThreeJetVec x, int index) {
  TwoJetVec result;
  result.x = D(x.x, index);
  result.y = D(x.y, index);
  result.z = D(x.z, index);
  return result;
}

ThreeJetVec Cross(ThreeJetVec v, ThreeJetVec w) {
  ThreeJetVec result;
  result.x = v.y*w.z + v.z*w.y*-1;
  result.y = v.z*w.x + v.x*w.z*-1;
  result.z = v.x*w.y + v.y*w.x*-1;
  return result;
}

ThreeJet Dot(ThreeJetVec v, ThreeJetVec w) {
  return v.x*w.x + v.y*w.y + v.z*w.z;
}

ThreeJetVec Normalize(ThreeJetVec v) {
  ThreeJet a;
  a = Dot(v,v);
  if (a > 0)
    a = a^-0.5;
  else
    a = ThreeJet(0, 0, 0);
  return v*a;
}

ThreeJetVec RotateZ(ThreeJetVec v, ThreeJet angle) {
  ThreeJetVec result;
  ThreeJet s,c;
  s = Sin (angle);
  c = Cos (angle);
  result.x =          v.x*c + v.y*s;
  result.y = v.x*s*-1 + v.y*c;
  result.z = v.z;
  return result;
}

ThreeJetVec RotateY(ThreeJetVec v, ThreeJet angle) {
  ThreeJetVec result;
  ThreeJet s, c;
  s = Sin (angle);
  c = Cos (angle);
  result.x = v.x*c + v.z*s*-1;
  result.y = v.y;
  result.z = v.x*s + v.z*c    ;
  return result;
}

ThreeJetVec RotateX(ThreeJetVec v, ThreeJet angle) {
  ThreeJetVec result;
  ThreeJet s,c;
  s = Sin (angle);
  c = Cos (angle);
  result.x = v.x;
  result.y = v.y*c + v.z*s;
  result.z = v.y*s*-1 + v.z*c;
  return result;
}

ThreeJetVec InterpolateVec(ThreeJetVec v1, ThreeJetVec v2, ThreeJet weight) {
  return (v1) * (weight*-1 + 1) + v2*weight;
}

ThreeJet Length(ThreeJetVec v)
{
  return (v.x^2 + v.y^2) ^ (.5);
}
