#ifndef __TWOJET
#define __TWOJET
#include <math.h>
extern "C" {
#include <stdio.h>
#include <stdlib.h>
}

class ThreeJet;
class TwoJet D(const class ThreeJet x, int index);
class TwoJet {
  public: /* this is a hack, but needed for now */
  double f;
  double fu, fv;
  double fuv;

  TwoJet() {}
  TwoJet(double d, double du, double dv)
   { f = d; fu = du; fv = dv; fuv = 0; }
  TwoJet(double d, double du, double dv, double duv)
   { f = d; fu = du; fv = dv; fuv = duv; }
  operator double() { return f; }
  operator<(double d) { return f < d; }
  operator>(double d) { return f > d; }
  operator>(int d) { return f > d; }
  operator<=(double d) { return f <= d; }
  operator>=(double d) { return f >= d; }
  double df_du() { return fu; }
  double df_dv() { return fv; }
  double d2f_dudv() { return fuv; }
  void operator +=(TwoJet x)
   { f += x.f; fu += x.fu; fv += x.fv; fuv += x.fuv; }
  void operator +=(double d)
   { f += d; }
  void operator *=(TwoJet x)
   {
     fuv = f*x.fuv + fu*x.fv + fv*x.fu + fuv*x.f;
     fu = f*x.fu + fu*x.f;
     fv = f*x.fv + fv*x.f;
     f *= x.f;
   }
  void operator *=(double d)
   { f *= d; fu *= d; fv *= d; fuv *= d; }
  void operator %=(double d)
   { f = fmod(f, d); if (f < 0) f += d; }
  void operator ^=(double n)
   {
    if (f > 0) {
     double x0 = pow(f, n);
     double x1 = n * x0/f;
     double x2 = (n-1)*x1/f;
     fuv = x1*fuv + x2*fu*fv;
     fu = x1*fu;
     fv = x1*fv;
     f = x0;
    }
   }
  void Annihilate(int index)
   { if (index == 0) fu = 0;
     else if (index == 1) fv = 0;
     fuv = 0;
   }
  void TakeSin() {
   *this *= 2*M_PI;
   double s = sin(f), c = cos(f);
   f = s; fu = fu*c; fv = fv*c; fuv = c*fuv - s*fu*fv;
  }
  void TakeCos() {
   *this *= 2*M_PI;
   double s = cos(f), c = -sin(f);
   f = s; fu = fu*c; fv = fv*c; fuv = c*fuv - s*fu*fv;
  }
  
  friend TwoJet operator+(const TwoJet x, const TwoJet y);
  friend TwoJet operator*(const TwoJet x, const TwoJet y);
  friend TwoJet operator+(const TwoJet x, double d);
  friend TwoJet operator+(const TwoJet x, int d) {return x+double(d);}
  friend TwoJet operator*(const TwoJet x, double d);
  friend TwoJet operator*(const TwoJet x, int d) {return x*double(d);}
  friend TwoJet Sin(const TwoJet x);
  friend TwoJet Cos(const TwoJet x);
  friend TwoJet operator^(const TwoJet x, double n);
  friend TwoJet Annihilate(const TwoJet x, int index);
  friend TwoJet Interpolate(const TwoJet v1, const TwoJet v2, const TwoJet weight);
  friend void printJet(const TwoJet);
  friend class TwoJet D(const class ThreeJet x, int index);
};

#endif
