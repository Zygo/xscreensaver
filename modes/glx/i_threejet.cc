#include "i_threejet.h"

ThreeJet operator+(const ThreeJet x, const ThreeJet y) {
  ThreeJet result;
  result.f = x.f + y.f;
  result.fu = x.fu + y.fu;
  result.fv = x.fv + y.fv;
  result.fuu = x.fuu + y.fuu;
  result.fuv = x.fuv + y.fuv;
  result.fvv = x.fvv + y.fvv;
  result.fuuv = x.fuuv + y.fuuv;
  result.fuvv = x.fuvv + y.fuvv;
  return result;
}

ThreeJet operator*(const ThreeJet x, const ThreeJet y) {
  ThreeJet result;
  result.f = x.f*y.f;
  result.fu = x.f*y.fu + x.fu*y.f;
  result.fv = x.f*y.fv + x.fv*y.f;
  result.fuu = x.f*y.fuu + 2*x.fu*y.fu + x.fuu*y.f;
  result.fuv = x.f*y.fuv + x.fu*y.fv + x.fv*y.fu + x.fuv*y.f;
  result.fvv = x.f*y.fvv + 2*x.fv*y.fv + x.fvv*y.f;
  result.fuuv = x.f*y.fuuv + 2*x.fu*y.fuv + x.fv*y.fuu
           + 2*x.fuv*y.fu + x.fuu*y.fv + x.fuuv*y.f;
  result.fuvv = x.f*y.fuvv + 2*x.fv*y.fuv + x.fu*y.fvv
           + 2*x.fuv*y.fv + x.fvv*y.fu + x.fuvv*y.f;
  return result;
}

ThreeJet operator+(const ThreeJet x, double d) {
  ThreeJet result;
  result = x;
  result.f += d;
  return result;
}

ThreeJet operator*(const ThreeJet x, double d) {
  ThreeJet result;
  result.f = d*x.f;
  result.fu = d*x.fu;
  result.fv = d*x.fv;
  result.fuu = d*x.fuu;
  result.fuv = d*x.fuv;
  result.fvv = d*x.fvv;
  result.fuuv = d*x.fuuv;
  result.fuvv = d*x.fuvv;
  return result;
}

ThreeJet Sin(const ThreeJet x) {
  ThreeJet result;
  ThreeJet t = x*double(2*M_PI);
  double s = sin(t.f);
  double c = cos(t.f);
  result.f = s;
  result.fu = c*t.fu;
  result.fv = c*t.fv;
  result.fuu = c*t.fuu - s*t.fu*t.fu;
  result.fuv = c*t.fuv - s*t.fu*t.fv;
  result.fvv = c*t.fvv - s*t.fv*t.fv;
  result.fuuv = c*t.fuuv - s*(2*t.fu*t.fuv + t.fv*t.fuu) - c*t.fu*t.fu*t.fv;
  result.fuvv = c*t.fuvv - s*(2*t.fv*t.fuv + t.fu*t.fvv) - c*t.fu*t.fv*t.fv;
  return result;
}

ThreeJet Cos(const ThreeJet x) {
  ThreeJet result;
  ThreeJet t = x*double(2*M_PI);
  double s = cos(t.f);
  double c = -sin(t.f);
  result.f = s;
  result.fu = c*t.fu;
  result.fv = c*t.fv;
  result.fuu = c*t.fuu - s*t.fu*t.fu;
  result.fuv = c*t.fuv - s*t.fu*t.fv;
  result.fvv = c*t.fvv - s*t.fv*t.fv;
  result.fuuv = c*t.fuuv - s*(2*t.fu*t.fuv + t.fv*t.fuu) - c*t.fu*t.fu*t.fv;
  result.fuvv = c*t.fuvv - s*(2*t.fv*t.fuv + t.fu*t.fvv) - c*t.fu*t.fv*t.fv;
  return result;
}

ThreeJet operator^(const ThreeJet x, double n) {
  double x0 = pow(x.f, n);
  double x1 = (x.f == 0) ? 0 : n * x0/x.f;
  double x2 = (x.f == 0) ? 0 : (n-1) * x1/x.f;
  double x3 = (x.f == 0) ? 0 : (n-2) * x2/x.f;
  ThreeJet result;
  result.f = x0;
  result.fu = x1*x.fu;
  result.fv = x1*x.fv;
  result.fuu = x1*x.fuu + x2*x.fu*x.fu;
  result.fuv = x1*x.fuv + x2*x.fu*x.fv;
  result.fvv = x1*x.fvv + x2*x.fv*x.fv;
  result.fuuv = x1*x.fuuv + x2*(2*x.fu*x.fuv + x.fv*x.fuu) + x3*x.fu*x.fu*x.fv;
  result.fuvv = x1*x.fuvv + x2*(2*x.fv*x.fuv + x.fu*x.fvv) + x3*x.fu*x.fv*x.fv;
  return result;
}

TwoJet D(const ThreeJet x, int index) {
  TwoJet result;
  if (index == 0) {
    result.f = x.fu;
    result.fu = x.fuu;
    result.fv = x.fuv;
    result.fuv = x.fuuv;
  } else if (index == 1) {
    result.f = x.fv;
    result.fu = x.fuv;
    result.fv = x.fvv;
    result.fuv = x.fuvv;
  } else {
    result.f = result.fu = result.fv =
    result.fuv = 0;
  }
  return result;
}

ThreeJet Annihilate(const ThreeJet x, int index) {
  ThreeJet result = ThreeJet(x.f,0,0);
  if (index == 0) {
    result.fv = x.fv;
    result.fvv = x.fvv;
  } else if (index == 1) {
    result.fu = x.fu;
    result.fuu = x.fuu;
  }
  return result;
}

ThreeJet Interpolate(const ThreeJet v1, const ThreeJet v2, const ThreeJet weight) {
  return (v1) * ((weight) * (-1) + 1) + v2*weight;
}

void printJet(const ThreeJet v) {
 (void) printf("%f (%f %f)\n",
  v.f,
  v.fu, v.fv
 );
}
