/* xscreensaver, Copyright (c) 1992 Jamie Zawinski <jwz@lucid.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This file contains some utility routines for randomly picking the colors
   to hack the screen with.
 */

#include <X11/Xlib.h>

void
hsv_to_rgb (h,s,v, r,g,b)
     int h;			/* 0 - 360   */
     double s, v;		/* 0.0 - 1.0 */
     unsigned short *r, *g, *b;	/* 0 - 65535 */
{
  double H, S, V, R, G, B;
  double p1, p2, p3;
  double f;
  int i;
  S = s; V = v;
  H = (h % 360) / 60.0;
  i = H;
  f = H - i;
  p1 = V * (1 - S);
  p2 = V * (1 - (S * f));
  p3 = V * (1 - (S * (1 - f)));
  if	  (i == 0) { R = V;  G = p3; B = p1; }
  else if (i == 1) { R = p2; G = V;  B = p1; }
  else if (i == 2) { R = p1; G = V;  B = p3; }
  else if (i == 3) { R = p1; G = p2; B = V;  }
  else if (i == 4) { R = p3; G = p1; B = V;  }
  else		   { R = V;  G = p1; B = p2; }
  *r = R * 65535;
  *g = G * 65535;
  *b = B * 65535;
}

void
rgb_to_hsv (r,g,b, h,s,v)
     unsigned short r, g, b;	/* 0 - 65535 */
     int *h;			/* 0 - 360   */
     double *s, *v;		/* 0.0 - 1.0 */
{
  double R, G, B, H, S, V;
  double cmax, cmin;
  double cmm;
  int imax;
  R = ((double) r) / 65535.0;
  G = ((double) g) / 65535.0;
  B = ((double) b) / 65535.0;
  cmax = R; cmin = G; imax = 1;
  if  ( cmax < G ) { cmax = G; cmin = R; imax = 2; }
  if  ( cmax < B ) { cmax = B; imax = 3; }
  if  ( cmin > B ) { cmin = B; }
  cmm = cmax - cmin;
  V = cmax;
  if (cmm == 0)
    S = H = 0;
  else
    {
      S = cmm / cmax;
      if      (imax == 1) H =       (G - B) / cmm;
      else if (imax == 2) H = 2.0 + (B - R) / cmm;
      else if (imax == 3) H = 4.0 + (R - G) / cmm;
      if (H < 0) H += 6.0;
    }
  *h = (H * 60.0);
  *s = S;
  *v = V;
}


void
make_color_ramp (h1, s1, v1, h2, s2, v2,
		 pixels, npixels)
     int h1, h2;			/* 0 - 360   */
     double s1, s2, v1, v2;		/* 0.0 - 1.0 */
     XColor *pixels;
     int npixels;
{
  int dh = (h2 - h1) / npixels;
  double ds = (s2 - s1) / npixels;
  double dv = (v2 - v1) / npixels;
  int i;
  for (i = 0; i < npixels; i++)
    hsv_to_rgb ((h1 += dh), (s1 += ds), (v1 += dv),
		&pixels [i].red, &pixels [i].green, &pixels [i].blue);
}


void
cycle_hue (color, degrees)
     XColor *color;
     int degrees;
{
  int h;
  double s, v;
  rgb_to_hsv (color->red, color->green, color->blue,
	      &h, &s, &v);
  h = (h + degrees) % 360;
  hsv_to_rgb (h, s, v, &color->red, &color->green, &color->blue);
}
