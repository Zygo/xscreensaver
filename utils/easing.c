/* Copyright © 2025 Jamie Zawinski <jwz@jwz.org>
 * Easing functions.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#include "utils.h"
#include "easing.h"

/* These are the same semantics as  CSS and jQuery. */

static double
easeInSine (double x)
{
  return 1 - cos ((x * M_PI) / 2);
}

static double
easeOutSine (double x)
{
  return sin ((x * M_PI) / 2);
}

static double
easeInOutSine (double x)
{
  return -(cos (M_PI * x) - 1) / 2;
}

static double
easeInQuad (double x)
{
  return x * x;
}

static double
easeOutQuad (double x)
{
  return 1 - (1 - x) * (1 - x);
}

static double
easeInOutQuad (double x)
{
  return (x < 0.5
          ? 2 * x * x
          : 1 - pow (-2 * x + 2, 2) / 2);
}

static double
easeInCubic (double x)
{
  return x * x * x;
}

static double
easeOutCubic (double x)
{
  return 1 - pow (1 - x, 3);
}

static double
easeInOutCubic (double x)
{
  return (x < 0.5
          ? 4 * x * x * x
          : 1 - pow (-2 * x + 2, 3) / 2);
}

static double
easeInQuart (double x)
{
  return x * x * x * x;
}

static double
easeOutQuart (double x)
{
  return 1 - pow (1 - x, 4);
}

static double
easeInOutQuart (double x)
{
  return (x < 0.5
          ? 8 * x * x * x * x
          : 1 - pow (-2 * x + 2, 4) / 2);
}

static double
easeInQuint (double x)
{
  return x * x * x * x * x;
}

static double
easeOutQuint (double x)
{
  return 1 - pow(1 - x, 5);
}

static double
easeInOutQuint (double x)
{
  return (x < 0.5
          ? 16 * x * x * x * x * x
          : 1 - pow (-2 * x + 2, 5) / 2);
}

static double
easeInExpo (double x)
{
  return x == 0 ? 0 : pow (2, 10 * x - 10);
}

static double
easeOutExpo (double x)
{
  return x == 1 ? 1 : 1 - pow (2, -10 * x);
}

static double
easeInOutExpo (double x)
{
  return (x == 0 ? 0 :
          x == 1 ? 1 :
          x < 0.5
          ? pow (2, 20 * x - 10) / 2
          : (2 - pow(2, -20 * x + 10)) / 2);
}

static double
easeInCirc (double x)
{
  return 1 - sqrt (1 - pow(x, 2));
}

static double
easeOutCirc (double x)
{
  return sqrt (1 - pow(x - 1, 2));
}

static double
easeInOutCirc (double x)
{
  return (x < 0.5
          ? (1 - sqrt(1 - pow (2 * x, 2))) / 2
          : (sqrt(1 - pow(-2 * x + 2, 2)) + 1) / 2);
}

static double
easeInBack (double x)
{
  double c1 = 1.70158;
  double c3 = c1 + 1;
  return c3 * x * x * x - c1 * x * x;
}

static double
easeOutBack (double x)
{
  double c1 = 1.70158;
  double c3 = c1 + 1;
  return 1 + c3 * pow (x - 1, 3) + c1 * pow(x - 1, 2);
}

static double
easeInOutBack (double x)
{
  double c1 = 1.70158;
  double c2 = c1 * 1.525;
  return (x < 0.5
          ? (pow (2 * x, 2) * ((c2 + 1) * 2 * x - c2)) / 2
          : (pow (2 * x - 2, 2) * ((c2 + 1) * (x * 2 - 2) + c2) + 2) / 2);
}

static double
easeInElastic (double x)
{
  double c4 = (2 * M_PI) / 3;
  return (x == 0 ? 0 :
          x == 1 ? 1 :
          -pow (2, 10 * x - 10) * sin ((x * 10 - 10.75) * c4));
}

static double
easeOutElastic (double x)
{
  double c4 = (2 * M_PI) / 3;
  return (x == 0 ? 0 :
          x == 1 ? 1 :
          pow (2, -10 * x) * sin ((x * 10 - 0.75) * c4) + 1);
}

static double
easeInOutElastic (double x)
{
  double c5 = (2 * M_PI) / 4.5;
  return (x == 0 ? 0 :
          x == 1 ? 1 :
          x < 0.5
          ? -(pow (2, 20 * x - 10) * sin ((20 * x - 11.125) * c5)) / 2
          :  (pow(2, -20 * x + 10) * sin ((20 * x - 11.125) * c5)) / 2 + 1);
}

static double
easeOutBounce (double x)
{
  double n1 = 7.5625;
  double d1 = 2.75;
  if (x < 1 / d1) {
    return n1 * x * x;
  } else if (x < 2 / d1) {
    x -= (1.5 / d1);
    return n1 * x * x + 0.75;
  } else if (x < 2.5 / d1) {
    x -= (2.25 / d1);
    return n1 * x * x + 0.9375;
  } else {
    x -= (2.625 / d1);
    return n1 * x * x + 0.984375;
  }
}

static double
easeInBounce (double x)
{
  return 1 - easeOutBounce (1 - x);
}

static double
easeInOutBounce (double x)
{
  return (x < 0.5
          ? (1 - easeOutBounce (1 - 2 * x)) / 2
          : (1 + easeOutBounce (2 * x - 1)) / 2);
}

double ease (easing_function fn, double x)
{
  switch (fn) {
  case EASE_NONE:	    return x;
  case EASE_IN_SINE:	    return easeInSine (x);
  case EASE_OUT_SINE:	    return easeOutSine (x);
  case EASE_IN_OUT_SINE:    return easeInOutSine (x);
  case EASE_IN_QUAD:	    return easeInQuad (x);
  case EASE_OUT_QUAD:	    return easeOutQuad (x);
  case EASE_IN_OUT_QUAD:    return easeInOutQuad (x);
  case EASE_IN_CUBIC:	    return easeInCubic (x);
  case EASE_OUT_CUBIC:	    return easeOutCubic (x);
  case EASE_IN_OUT_CUBIC:   return easeInOutCubic (x);
  case EASE_IN_QUART:	    return easeInQuart (x);
  case EASE_OUT_QUART:	    return easeOutQuart (x);
  case EASE_IN_OUT_QUART:   return easeInOutQuart (x);
  case EASE_IN_QUINT:	    return easeInQuint (x);
  case EASE_OUT_QUINT:	    return easeOutQuint (x);
  case EASE_IN_OUT_QUINT:   return easeInOutQuint (x);
  case EASE_IN_EXPO:	    return easeInExpo (x);
  case EASE_OUT_EXPO:	    return easeOutExpo (x);
  case EASE_IN_OUT_EXPO:    return easeInOutExpo (x);
  case EASE_IN_CIRC:	    return easeInCirc (x);
  case EASE_OUT_CIRC:	    return easeOutCirc (x);
  case EASE_IN_OUT_CIRC:    return easeInOutCirc (x);
  case EASE_IN_BACK:	    return easeInBack (x);
  case EASE_OUT_BACK:	    return easeOutBack (x);
  case EASE_IN_OUT_BACK:    return easeInOutBack (x);
  case EASE_IN_ELASTIC:	    return easeInElastic (x);
  case EASE_OUT_ELASTIC:    return easeOutElastic (x);
  case EASE_IN_OUT_ELASTIC: return easeInOutElastic (x);
  case EASE_IN_BOUNCE:	    return easeInBounce (x);
  case EASE_OUT_BOUNCE:	    return easeOutBounce (x);
  case EASE_IN_OUT_BOUNCE:  return easeInOutBounce (x);
  default:                  abort();
  }
}
