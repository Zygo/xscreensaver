#ifndef __THREEJET
#define __THREEJET
#include "i_twojet.h"

#define DIM 2
class ThreeJet {
  double f;
  double fu, fv;
  double fuu, fuv, fvv;
  double fuuv, fuvv;

  ThreeJet(double d, double du, double dv, double duu, double duv, double dvv,
   double duuv, double duvv)
   { f = d; fu = du; fv = dv; fuu = duu; fuv = duv; fvv = dvv;
     fuuv = duuv; fuvv = duvv; }
  public:
  ThreeJet() {}
  ThreeJet(double d, double du, double dv)
   { f = d; fu = du; fv = dv; fuu = fuv = fvv = fuuv = fuvv = 0;}
  operator TwoJet() { return TwoJet(f, fu, fv, fuv); }
  operator double() { return f; }
  Bool operator<(double d) { return f < d; }
  Bool operator<(int d) { return f < d; }
  Bool operator>(double d) { return f > d; }
  Bool operator>(int d) { return f > d; }
  Bool operator<=(double d) { return f <= d; }
  Bool operator<=(int d) { return f <= d; }
  Bool operator>=(double d) { return f >= d; }
  void operator %=(double d)
   { f = fmod(f, d); if (f < 0) f += d; }
  friend ThreeJet operator+(const ThreeJet x, const ThreeJet y);
  friend ThreeJet operator*(const ThreeJet x, const ThreeJet y);
  friend ThreeJet operator+(const ThreeJet x, double d);
  friend ThreeJet operator+(const ThreeJet x, int d) {return x+double(d);}
  friend ThreeJet operator*(const ThreeJet x, double d);
  friend ThreeJet operator*(const ThreeJet x, int d) {return x*double(d);}
  friend ThreeJet Sin(const ThreeJet x);
  friend ThreeJet Cos(const ThreeJet x);
  friend ThreeJet operator^(const ThreeJet x, double n);
  friend ThreeJet Annihilate(const ThreeJet x, int index);
  friend ThreeJet Interpolate(const ThreeJet v1, const ThreeJet v2, const ThreeJet weight);
  friend void printJet(const ThreeJet);
  friend class TwoJet D(const class ThreeJet x, int index);
};

#endif
