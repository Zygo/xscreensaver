#ifndef __FIGUREEIGHT
#define __FIGUREEIGHT
#include "threejetvec.h"
#include "twojetvec.h"

#define N_STRIPS 8

TwoJetVec FigureEight(
  TwoJetVec w, TwoJetVec h, TwoJetVec bend,
  TwoJet form, TwoJet v
);
TwoJetVec AddFigureEight(ThreeJetVec p, ThreeJet u, TwoJet v, ThreeJet form, ThreeJet scale);

#endif
