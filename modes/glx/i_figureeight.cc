#include "i_figureeight.h"

extern int n_strips;

TwoJetVec FigureEight(
  TwoJetVec w, TwoJetVec h, TwoJetVec bend, TwoJet form, TwoJet v
) {
  TwoJet height;
  v %= 1;
  height = (Cos (v*2) + -1) * (-1);
  if (v > 0.25 && v < 0.75)
    height = height*-1 + 4;
  height = height*0.6;
  h = h + bend*(height*height*(1/64.));
  return
    w*Sin (v*2) + (h) * (Interpolate((Cos (v) + -1) * (-2), height, form))
  ;
}

TwoJetVec AddFigureEight(ThreeJetVec p, ThreeJet u, TwoJet v, ThreeJet form, ThreeJet scale)
{
  ThreeJet size = form*scale;
  form = form*2 + form*form*-1;
  TwoJetVec dv = AnnihilateVec(D(p, 1), 1);
  p = AnnihilateVec(p, 1);
  TwoJetVec du = Normalize(D(p, 0));
  TwoJetVec h = Normalize(Cross(du, dv))*TwoJet(size);
  TwoJetVec w = Normalize(Cross(h, du))*(TwoJet(size)*1.1);
  return RotateZ(
    TwoJetVec(p) +
      FigureEight(w, h, du*D(size, 0)*(D(u, 0)^(-1)), form, v),
    v*(1./n_strips)
  );
}
