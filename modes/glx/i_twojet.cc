#include "i_twojet.h"

TwoJet operator+(const TwoJet x, const TwoJet y) {
  return TwoJet(x.f+y.f, x.fu+y.fu, x.fv+y.fv, x.fuv + y.fuv);
}

TwoJet operator*(const TwoJet x, const TwoJet y) {
  return TwoJet(
    x.f*y.f,
    x.f*y.fu + x.fu*y.f,
    x.f*y.fv + x.fv*y.f,
    x.f*y.fuv + x.fu*y.fv + x.fv*y.fu + x.fuv*y.f
  );
}

TwoJet operator+(const TwoJet x, double d) {
  return TwoJet( x.f + d, x.fu, x.fv, x.fuv);
}

TwoJet operator*(const TwoJet x, double d) {
  return TwoJet( d*x.f, d*x.fu, d*x.fv, d*x.fuv);
}

TwoJet Sin(const TwoJet x) {
  TwoJet t = x*double(2*M_PI);
  double s = sin(t.f);
  double c = cos(t.f);
  return TwoJet(s, c*t.fu, c*t.fv, c*t.fuv - s*t.fu*t.fv);
}

TwoJet Cos(const TwoJet x) {
  TwoJet t = x*double(2*M_PI);
  double s = cos(t.f);
  double c = -sin(t.f);
  return TwoJet(s, c*t.fu, c*t.fv, c*t.fuv - s*t.fu*t.fv);
}

TwoJet operator^(const TwoJet x, double n) {
  double x0 = pow(x.f, n);
  double x1 = (x.f == 0) ? 0 : n * x0/x.f;
  double x2 = (x.f == 0) ? 0 : (n-1)*x1/x.f;
  return TwoJet(x0, x1*x.fu, x1*x.fv, x1*x.fuv + x2*x.fu*x.fv);
}

TwoJet Annihilate(const TwoJet x, int index) {
  return TwoJet(x.f, index == 1 ? x.fu : 0, index == 0 ? x.fv : 0, 0);
}

TwoJet Interpolate(const TwoJet v1, const TwoJet v2, const TwoJet weight) {
  return (v1) * ((weight) * (-1) + 1) + v2*weight;
}

void printJet(const TwoJet v) {
 printf("%f (%f %f)\n",
  v.f,
  v.fu, v.fv
 );
}
